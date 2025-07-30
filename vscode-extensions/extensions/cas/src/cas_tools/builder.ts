import { isSubpath } from "@cas/helpers";
import { CompCommandEntry, CompCommands } from "@cas/types/bas.js";
import { getLogger } from "@logtape/logtape";
import { createWriteStream, existsSync, readFileSync } from "fs";
import * as path from "path";
import { join } from "shlex";
import * as vscode from "vscode";
import {
	LanguageClient,
	LanguageClientOptions,
	ServerOptions,
	TransportKind,
} from "vscode-languageclient/node";
import { DBProvider } from "../db";
import { Settings } from "../settings";

export class Builder implements vscode.Disposable {
	s: Settings;
	ctx: vscode.ExtensionContext;
	db: DBProvider;
	client: LanguageClient | undefined = undefined;
	name = "CAS Build Verifier";
	private readonly logger = getLogger(["CAS", "tools", "builder"]);

	constructor(
		context: vscode.ExtensionContext,
		settings: Settings,
		db: DBProvider,
	) {
		this.s = settings;
		this.ctx = context;
		this.db = db;
		this.logger.debug`Initializing Builder`;

		let serverModule = context.asAbsolutePath(
			path.join("dist", "cas_tools", "build_server.js"),
		);

		let debugOptions = { execArgv: ["--nolazy", "--inspect=6009"] };

		let serverOptions: ServerOptions = {
			run: { module: serverModule, transport: TransportKind.ipc },
			debug: {
				module: serverModule,
				transport: TransportKind.ipc,
				options: debugOptions,
			},
		};

		let clientOptions: LanguageClientOptions = {
			diagnosticCollectionName: this.name,
			documentSelector: [
				{ scheme: "file", language: "c" },
				{ scheme: "file", language: "cpp" },
			],
			diagnosticPullOptions: {
				onSave: true,
			},
			synchronize: {
				fileEvents: [
					vscode.workspace.createFileSystemWatcher("**/compile_commands.json"),
					vscode.workspace.createFileSystemWatcher("**/rdc.json"),
				],
			},
			initializationOptions: {
				baseDir: this.s.baseDir,
				concurency: this.s.concurency,
			},
		};
		this.client = new LanguageClient(
			"cas",
			this.name,
			serverOptions,
			clientOptions,
		);

		this.client.registerProposedFeatures();
		this.client.start();

		this.logger.debug`Builder initialization complete`;
		if (this.client) {
			context.subscriptions.push(
				vscode.commands.registerCommand("cas.verify.workspace", () => {
					this.verifyWorkspace();
				}),
			);
			context.subscriptions.push(
				vscode.commands.registerCommand("cas.verify.single", (param) => {
					this.verifySingle(param.path);
				}),
			);
			context.subscriptions.push(
				vscode.commands.registerCommand(
					"cas.verify.generate.script.sh",
					(uri?: vscode.Uri) => {
						this.generateScriptSh(uri);
					},
				),
			);
			context.subscriptions.push(
				vscode.commands.registerCommand(
					"cas.verify.generate.script.mk",
					(uri?: vscode.Uri) => {
						this.generateScriptMk(uri);
					},
				),
			);
		}
	}

	dispose() {
		this.client?.stop().then(() => {
			this.client?.dispose();
		});
	}

	async verifySingle(file: string) {
		await vscode.window.withProgress(
			{
				location: vscode.ProgressLocation.Notification,
				title: "Analyzing compile command ... \n",
				cancellable: true,
			},
			async (_progress, token) => {
				await this.client
					?.sendRequest<number>(
						"server/verifySingle",
						{ file_name: file },
						token,
					)
					.then((timeMs: number) => {
						vscode.window.showInformationMessage(
							`Analysis finished in ${new Date(timeMs).toISOString().slice(11, 19)}`,
						);
					});
				this.logger.debug`Single file verification completed`;
			},
		);
	}

	async verifyWorkspace() {
		this.logger.debug`Starting workspace verification`;
		const compCommandsFile: vscode.Uri | undefined =
			await this.getCompCommandsFilename();
		if (compCommandsFile) {
			this.logger.debug`Loading compile commands from ${compCommandsFile}`;
			let comps = JSON.parse(
				readFileSync(compCommandsFile.fsPath, "utf-8").replaceAll(
					"${workspaceFolder}",
					Settings.getWorkspaceDir(),
				),
			).map((a: any) => path.normalize(a.file));
			if (comps) {
				this.logger.debug`Compile commands loaded successfully`;

				await vscode.window.withProgress(
					{
						location: vscode.ProgressLocation.Notification,
						title: "Analyzing compile commands ... \n",
						cancellable: true,
					},
					async (progress, token) => {
						let oneItemPercent = (1 / comps.length) * 100;
						let lastIndexed = 1;

						this.client?.onRequest("client/progress", () => {
							progress.report({
								message: `${lastIndexed++} / ${comps.length}`,
								increment: oneItemPercent,
							});
						});

						await this.client
							?.sendRequest<number>("server/verifyAll", token)
							.then((timeMs: number) => {
								vscode.window.showInformationMessage(
									`Analysis finished in ${new Date(timeMs).toISOString().slice(11, 19)}`,
								);
							});
						this.logger.debug`Workspace verification completed`;
					},
				);
			} else {
				vscode.window.showErrorMessage(
					"Error - can't load compile_commands.json!",
				);
				return;
			}
		} else {
			vscode.window.showErrorMessage(
				"Error - can't find compile_commands.json in workspace!",
			);
			return;
		}
	}

	async getCompCommandsFilename(): Promise<vscode.Uri | undefined> {
		let ret = undefined;
		const expectedCCFiles = await vscode.workspace.findFiles(
			"compile_commands.json",
		);
		expectedCCFiles.forEach((f) => {
			if (existsSync(f.fsPath)) {
				ret = f;
			}
		});
		return ret;
	}

	async generateScriptSh(uri?: vscode.Uri) {
		this.logger.debug`Generating shell script`;
		const compCommandsFile: vscode.Uri | undefined =
			await this.getCompCommandsFilename();
		if (compCommandsFile) {
			this.logger.debug`Loading compile commands from ${compCommandsFile}`;
			let wsdir = Settings.getWorkspaceDir();
			let comps: CompCommands = JSON.parse(
				readFileSync(compCommandsFile.fsPath, "utf-8").replaceAll(
					"${workspaceFolder}",
					wsdir,
				),
			);
			const ofStream = createWriteStream(
				path.join(Settings.getWorkspaceDir(), "shell.sh"),
				{ flags: "w" },
			);
			comps
				.filter(
					(comp: CompCommandEntry) => !uri || isSubpath(uri.fsPath, comp.file),
				)
				.forEach((comp: CompCommandEntry) => {
					let cmd = JSON.stringify(
						"arguments" in comp ? join(comp.arguments) : comp.command,
					);
					cmd = cmd.substring(1, cmd.length - 1);
					cmd = cmd.replaceAll("(", "\\(").replaceAll(")", "\\)");
					ofStream.write(
						`cd ${comp.directory} && ${cmd} || echo "***${comp.file} - FAIL**"\n\n`,
					);
				});
			this.logger.debug`Shell script generation completed`;
		}
	}

	async generateScriptMk(uri?: vscode.Uri) {
		this.logger.debug`Generating makefile`;
		const compCommandsFile: vscode.Uri | undefined =
			await this.getCompCommandsFilename();
		if (compCommandsFile) {
			this.logger.debug(
				(l) => l`Loading compile commands from ${compCommandsFile}`,
			);
			let wsdir = Settings.getWorkspaceDir();
			let comps: CompCommands = JSON.parse(
				readFileSync(compCommandsFile.fsPath, "utf-8").replaceAll(
					"${workspaceFolder}",
					wsdir,
				),
			);
			const ofStream = createWriteStream(
				path.join(Settings.getWorkspaceDir(), "makefile"),
				{ flags: "w" },
			);
			ofStream.write(".PHONY: all\n");
			let compsArr = Array.from(
				{ length: comps.length },
				(_, i) => "comp_" + i,
			);
			for (let index = 0; index < compsArr.length; index++) {
				ofStream.write(`.PHONY: ${compsArr[index]}\n`);
			}
			ofStream.write("all: " + compsArr.join(" ") + "\n\n");
			for (let index = 0; index < comps.length; index++) {
				let comp: CompCommandEntry = comps[index];
				if (uri && !isSubpath(uri.fsPath, comp.file)) {
					continue;
				}
				let cmd = JSON.stringify(
					"arguments" in comp ? join(comp.arguments) : comp.command,
				);
				cmd = cmd.substring(1, cmd.length - 1);
				cmd = cmd.replaceAll("(", "\\(").replaceAll(")", "\\)");
				ofStream.write(
					`comp_${index}:\n\t@(cd ${comp.directory} && ${cmd} )\n\n`,
				);
			}
			this.logger.debug`Makefile generation completed`;
		}
	}
}
