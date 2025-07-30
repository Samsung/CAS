import { DepsSchema } from "@cas/deps";
import { decodeText } from "@cas/helpers";
import { testAccess, withTrailingSlash } from "@cas/helpers/fs.js";
import { deepMerge } from "@cas/helpers/object.js";
import { Manifest } from "@cas/manifest";
import { CompCommands } from "@cas/types/bas.js";
import { Paged } from "@cas/types/cas_server.js";
import { getLogger } from "@logtape/logtape";
import { mkdir, open, readdir, rm, rmdir, writeFile } from "fs/promises";
import { JsonStreamStringify } from "json-stream-stringify";
import { join, normalize } from "path";
import { split } from "shlex";
import { pipeline } from "stream/promises";
import * as vscode from "vscode";
import { DBProvider } from "../db/index.js";
import { Settings } from "../settings.js";
import { DepsGenerator, DepsGeneratorOptions } from "./generators/generator.js";

import localGenerator from "./generators/local.js";
import NotImplementedGenerator from "./generators/notImplemented.js";
import p4Generator from "./generators/p4.js";
import sshGenerator from "./generators/ssh.js";
import { ManifestSettings } from "./manifest.js";

const logger = getLogger(["CAS", "workspace", "generator"]);
export class WorkspaceGenerator {
	ctx: vscode.ExtensionContext;
	dbProvider: DBProvider;
	s: Settings;
	ms: ManifestSettings;
	private static workspaceGeneratedEventEmitter =
		new vscode.EventEmitter<void>();
	static workspaceGenerated = this.workspaceGeneratedEventEmitter.event;
	constructor(
		context: vscode.ExtensionContext,
		dbProvider: DBProvider,
		settings: Settings,
		manigestSettings: ManifestSettings,
	) {
		this.ctx = context;
		this.s = settings;
		this.ms = manigestSettings;
		this.dbProvider = dbProvider;
		if (this.s.isGenerated) {
			context.subscriptions.push(
				vscode.commands.registerCommand("cas.ws.update", () =>
					this.updateWorkspace(),
				),
			);
		}
	}

	readonly depsGenerators: Record<
		Manifest["sourceRepo"]["type"],
		DepsGenerator
	> = {
		local: localGenerator,
		sftp: sshGenerator,
		p4: p4Generator,
		archive: NotImplementedGenerator,
		repo: NotImplementedGenerator,
	};

	getInputBox() {
		return vscode.window.showInputBox({
			ignoreFocusOut: true,
			placeHolder: "directory name",
			prompt: "Input multi project directory name",
			value: "multi_project",
		});
	}

	async removeEmptyDirectories(directory: string) {
		let paths = await readdir(directory, {
			withFileTypes: true,
			encoding: "utf-8",
		});
		if (paths.length === 0) {
			await rmdir(directory, { recursive: false });
		} else {
			for (let index = 0; index < paths.length; index++) {
				if (paths[index].isDirectory()) {
					this.removeEmptyDirectories(
						join(paths[index].parentPath, paths[index].name),
					);
				}
			}
			paths = await readdir(directory, {
				withFileTypes: true,
				encoding: "utf-8",
			});
			if (paths.length === 0) {
				await rmdir(directory, { recursive: false });
			}
		}
	}

	isLocal() {
		return (
			this.s.workspaceType === "type:local" &&
			(!this.ms.initialized || this.ms.sourceRepoType === "local")
		);
	}

	async getDepsGenerator(): Promise<DepsGenerator> {
		const type = this.ms.manifest?.sourceRepo.type ?? "local";
		return this.depsGenerators[type];
	}

	async updateWorkspace(
		inputCmd: string | undefined = undefined,
		paths: string[] = [],
	) {
		this.generateWorkspace(inputCmd, undefined, paths, true);
	}

	async generateWorkspace(
		cmd?: string,
		projectName?: string,
		paths: string[] = [],
		update = false,
	) {
		cmd ??= this.s.genCmd;
		if (!cmd) {
			logger.error`No command defined`;
			throw new Error("No command defined");
		}
		projectName ??= this.s.projName;
		if (!projectName) {
			logger.error`No project name`;
			throw new Error("No project name");
		}
		const createDeps = await this.getDepsGenerator();
		let currentStep = 1;
		const steps =
			(createDeps.createSteps ?? 0) +
			2 +
			(update ? (createDeps.updateSteps ?? 0) : 0);
		const db = this.dbProvider.getDB();
		if (!db) {
			logger.error`No BAS Database`;
			throw new Error("No BAS Database");
		}
		await vscode.window.withProgress(
			{
				location: vscode.ProgressLocation.Notification,
				title: "CAS Extension",
				cancellable: true,
			},
			async (progress) => {
				progress.report({
					message: `Generating dependencies ${currentStep++}/${steps}...`,
				});
				const deps = await db
					.runCmd<DepsSchema>(cmd + " --details --relative -n=0", true)
					.then((deps) => {
						if (!deps) {
							throw new Error("No dependencies found");
						}
						if ("ERROR" in deps) {
							throw new Error(Object.values(deps).join(" "));
						}
						return deps;
					})
					.catch((err) => {
						logger.error`Error generating dependencies: ${err.message}`;
						throw err;
					});
				const binaries = (await db.runCmd<Paged<string>>("binaries -n=0", true))
					.entries;
				const sourceRoot = await db
					.runCmd<{ source_root: string }>("source_root", true)
					.then((value) => {
						if (!value) {
							throw new Error("No source root found");
						}
						return value.source_root;
					})
					.catch((err) => {
						logger.error`Error getting source root: ${err.message}`;
						throw err;
					});
				const out = update
					? Settings.getWorkspaceDir()
					: withTrailingSlash(
							join(Settings.getWorkspaceDir(), ".cas", projectName),
						);
				const depsOptions: DepsGeneratorOptions = {
					cmd,
					deps,
					binaries,
					out,
					paths,
					sourceRoot,
					update,
					settings: this.s,
					manifest: this.ms.manifest,
					ctx: this.ctx,
					dbProvider: this.dbProvider,
					reportProgress: ({ message, increment }) =>
						progress.report({
							message: message
								? `${message} ${currentStep++}/${steps}...`
								: undefined,
							increment,
						}),
				};
				if (!update && (await testAccess(out)) && (await readdir(out)).length) {
					const result = await vscode.window.showQuickPick(["Yes", "No"], {
						title: `${out} already exists, do you want to continue? (warning: this will delete that directory)`,
					});
					if (result === "No") {
						throw new Error("Directory exists");
					}
					await rm(out, { recursive: true, force: true });
				} else if (update) {
					await createDeps.update(depsOptions);
				}
				await mkdir(out, { recursive: true });
				const {
					deps: modifiedDeps,
					depsFolders,
					additionalManifestData: additionalSettings,
					additionalManifestKeys: additionalManifestFieldKeys,
				} = await createDeps.createDeps(depsOptions);

				const depsFile = await open(join(out, "deps.json"), "w");
				if (modifiedDeps.num_entries > 5000) {
					// for large deps files use a streaming writer to avoid hitting the string length limit
					const t = Date.now();
					logger.debug`Starting writing deps file`;
					await pipeline([
						new JsonStreamStringify(modifiedDeps),
						depsFile.createWriteStream({ encoding: "utf-8" }),
					]);
					logger.debug`Writing deps file took ${Date.now() - t}ms`;
				} else {
					// for smaller files we can afford to "prettify" them with spaces
					await depsFile.writeFile(JSON.stringify(modifiedDeps, null, 2), {
						encoding: "utf-8",
					});
				}
				await depsFile.close();

				progress.report({
					message: `Processing compile commands ${currentStep++}/${steps}...`,
				});
				const compCommands = await this.getCompileCommands(
					cmd + " --commands --generate --original-path -n=0",
					sourceRoot,
					out,
				);
				logger.debug`Compile commands length: ${compCommands.length}`;
				const ccFile = join(out, "compile_commands.json");
				const modulesFile = join(out, "modules.json");
				const wsFile = join(out, `${projectName}.code-workspace`);
				// write all files
				await Promise.all([
					writeFile(ccFile, JSON.stringify(compCommands, null, 2)),
					writeFile(modulesFile, JSON.stringify(paths, null, 2)),
					this.writeWS(
						projectName,
						ccFile,
						wsFile,
						out,
						cmd,
						depsFolders,
						additionalManifestFieldKeys,
						additionalSettings,
					),
				]);
				WorkspaceGenerator.workspaceGeneratedEventEmitter.fire();
				vscode.commands.executeCommand(
					"vscode.openFolder",
					vscode.Uri.file(wsFile),
					true,
				);
			},
		);
	}

	async getCompileCommands(
		cmd: string,
		sourceRoot: string,
		outRoot: string,
	): Promise<any[]> {
		let compCommands: CompCommands | undefined = await this.dbProvider
			.getDB()
			?.getResponse(cmd);
		if (compCommands) {
			let s = normalize(sourceRoot);
			s = s.endsWith("/") ? s.slice(0, -1) : s;
			let o = normalize(outRoot);
			o = o.endsWith("/") ? o.slice(0, -1) : o;
			for (let index = 0; index < compCommands.length; index++) {
				const currentCommand = compCommands[index];
				compCommands[index] = {
					arguments: ("arguments" in currentCommand
						? currentCommand.arguments
						: split(currentCommand.command)
					).map((x) => x.replaceAll(s, o)),
					directory: currentCommand.directory.replaceAll(s, o),
					file: currentCommand.file.replaceAll(s, o),
				};
			}
			return compCommands;
		} else {
			return [];
		}
	}

	async writeWS(
		projectName: string,
		ccFile: string,
		wsFile: string,
		sourceRoot: string,
		generationCmd: string,
		depsFolders?: string[],
		additionalManifestFieldKeys?: (keyof Manifest)[],
		additionalManifestData?: Record<string, any>,
	) {
		const currentSettings = await this.getCurrentWorkspaceSettings();
		const workspaceJson = deepMerge(
			{
				...currentSettings,
				folders: [{ path: sourceRoot }],
				settings: {
					...(currentSettings?.settings ?? {}),
					"cas.manifest": this.ms.workspaceManifest([
						"version",
						"basProvider",
						"ftdbProvider",
						"sourceRepo",
						"opengrok",
						...(additionalManifestFieldKeys ?? []),
					]),
					"cas.server": this.s.casServer,
					"cas.serverPortRange": this.s.casServerPortRange,
					"cas.basDatabase": this.s.basDB,
					"cas.basDatabases": this.s.basDBs,
					"cas.ftdbDatabase": this.s.FTDB,
					"cas.ftdbDatabases": this.s.FTDBs,
					"cas.baseDir": Settings.getWorkspaceDir(),
					"cas.genCmd": generationCmd,
					"cas.projectName": projectName,
					"cas.depsFolders": depsFolders,
					"C_Cpp.default.compileCommands": ccFile,
					"C_Cpp.default.includePath": [sourceRoot],
				},
			},
			additionalManifestData ?? {},
		);
		return writeFile(wsFile, JSON.stringify(workspaceJson, null, 2));
	}
	async getCurrentWorkspaceSettings() {
		if (vscode.workspace.workspaceFile) {
			try {
				return JSON.parse(
					decodeText(
						await vscode.workspace.fs.readFile(vscode.workspace.workspaceFile),
					),
				);
			} catch {
				return {};
			}
		}
		return {};
	}
}
