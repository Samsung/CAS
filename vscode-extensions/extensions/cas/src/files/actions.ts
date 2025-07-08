import { CommandInfo, FileMode } from "@cas/types/bas.js";
import { Paged } from "@cas/types/cas_server.js";
import { VSCodeContext } from "@cas/types/webview.js";
import { relativePath } from "@cas/vscode-variables/helpers.js";
import { join } from "path";
import {
	CancellationToken,
	CodeLens,
	CodeLensProvider,
	Command,
	commands,
	Event,
	EventEmitter,
	ExtensionContext,
	Position,
	ProviderResult,
	Range,
	TextDocument,
	Uri,
	window,
	workspace,
} from "vscode";
import { DBProvider } from "../db";
import { FileData } from "./file_data";

type FileArg = Uri | VSCodeContext | string;

type LensType =
	| "rev_hierarchy"
	| "file_info"
	| "file_deps"
	| "faccess"
	| "moddeps"
	| "revcomps"
	| "linked_modules"
	| "compilation_info";

export class ActionProvider implements CodeLensProvider {
	fileData: Promise<FileData>;
	db: DBProvider;
	readonly lensActions: Partial<Record<LensType, Command>> = {
		rev_hierarchy: {
			command: "cas.file.revdeps",
			title: "Reverse Dependencies",
		},
		faccess: {
			command: "cas.file.faccess",
			title: "Accessing processes",
		},
		file_deps: {
			command: "cas.file.deps",
			title: "Dependencies",
		},
		moddeps: {
			command: "cas.file.moddeps",
			title: "Module Dependencies",
		},
		revcomps: {
			command: "cas.file.revcomps",
			title: "Reverse compilation dependencies",
		},
		compilation_info: {
			command: "cas.file.compilation_info",
			title: "Compilation info",
		},
	};
	constructor(context: ExtensionContext, db: DBProvider) {
		this.fileData = FileData.init(context, db);
		this.db = db;
		context.subscriptions.push(
			commands.registerCommand("cas.file.revdeps", (file?: FileArg) =>
				this.revHierarchy(file ?? window.activeTextEditor?.document.uri!),
			),
			commands.registerCommand("cas.file.revdeps.recursive", (file?: FileArg) =>
				this.revHierarchy(file ?? window.activeTextEditor?.document.uri!, true),
			),
			commands.registerCommand("cas.file.faccess", (file?: FileArg) =>
				this.faccess(file ?? window.activeTextEditor?.document.uri!),
			),
			commands.registerCommand("cas.file.deps", (file?: FileArg) =>
				this.fileDeps(file ?? window.activeTextEditor?.document.uri!),
			),
			commands.registerCommand("cas.file.deps.direct", (file?: FileArg) =>
				this.fileDeps(file ?? window.activeTextEditor?.document.uri!, true),
			),
			commands.registerCommand("cas.file.moddeps", (file?: FileArg) =>
				this.modDeps(file ?? window.activeTextEditor?.document.uri!),
			),
			commands.registerCommand("cas.file.moddeps.direct", (file?: FileArg) =>
				this.modDeps(file ?? window.activeTextEditor?.document.uri!, true),
			),
			commands.registerCommand("cas.file.revcomps", (file?: FileArg) =>
				this.revcompsFor(file ?? window.activeTextEditor?.document.uri!),
			),
			commands.registerCommand("cas.file.compilation_info", (file?: FileArg) =>
				this.compilationInfo(file ?? window.activeTextEditor?.document.uri!),
			),
			commands.registerCommand("cas.file.opens", (file?: FileArg) =>
				this.opens(file ?? window.activeTextEditor?.document.uri!),
			),
		);
	}
	changedCodeLenses = new EventEmitter<void>();
	onDidChangeCodeLenses = this.changedCodeLenses.event;

	lenses = new WeakMap<CodeLens, LensType>();

	async provideCodeLenses(
		document: TextDocument,
		_token: CancellationToken,
	): Promise<CodeLens[]> {
		const data = (await this.fileData).get(document.uri);
		if (!data) {
			commands.executeCommand("setContext", "casFile", false);
			return [];
		}
		commands.executeCommand("setContext", "casFile", true);
		const lenses: LensType[] = [
			"rev_hierarchy",
			"file_info",
			"faccess",
			"revcomps",
		];
		if (data.access === "w") {
			lenses.push("file_deps");
		}
		if (data.type === "linked") {
			lenses.push("moddeps");
		}
		if (data.type === "compiled") {
			lenses.push("compilation_info");
		}
		return lenses.map((type) => {
			const lens = new CodeLens(
				document.validateRange(
					new Range(
						new Position(0, 0),
						new Position(document.lineCount, Infinity),
					),
				),
			);
			this.lenses.set(lens, type);
			return lens;
		});
	}
	async resolveCodeLens(
		codeLens: CodeLens,
		_token: CancellationToken,
	): Promise<CodeLens> {
		const type = this.lenses.get(codeLens);
		if (!type) {
			return codeLens;
		}
		codeLens.command = this.lensActions[type];
		return codeLens;
	}

	async toSourceRootPath(uri: FileArg) {
		if (typeof uri === "string") {
			return (
				(await this.fileData).getString(uri)?.original_path ??
				relativePath(
					uri,
					workspace.getWorkspaceFolder(Uri.file(uri))?.uri?.fsPath ?? ".",
				)
			);
		} else if ("v" in uri) {
			return uri.v;
		}
		return (
			(await this.fileData).get(uri)?.original_path ??
			relativePath(uri.fsPath, workspace.getWorkspaceFolder(uri)!.uri.fsPath)
		);
	}

	async revHierarchy(file: FileArg, recursive = false) {
		return commands.executeCommand(
			"cas.cmd.sendCommand",
			`revdeps_for --path=${await this.toSourceRootPath(file)} --details ${recursive ? "--recursive" : ""}`,
		);
	}
	async faccess(file: FileArg) {
		return commands.executeCommand(
			"cas.view.procTree",
			(
				await this.db
					.getDB()
					?.runCmd<Paged<CommandInfo>>(
						`faccess --path=${await this.toSourceRootPath(file)} --commands -n 0`,
						true,
						false,
					)
			)?.entries.map(({ pid, idx }) => [pid, idx]),
		);
	}
	async fileDeps(file: FileArg, direct = false) {
		return commands.executeCommand(
			"cas.cmd.sendCommand",
			`deps_for --path=${await this.toSourceRootPath(file)} --details ${direct ? "--direct" : ""}`,
		);
	}
	async modDeps(file: FileArg, direct = false) {
		return commands.executeCommand(
			"cas.cmd.sendCommand",
			`moddeps_for --path=${await this.toSourceRootPath(file)} --details ${direct ? "--direct" : ""}`,
		);
	}
	async revcompsFor(file: FileArg) {
		return commands.executeCommand(
			"cas.cmd.sendCommand",
			`revcomps_for --path=${await this.toSourceRootPath(file)} --details`,
		);
	}
	async compilationInfo(file: FileArg) {
		return commands.executeCommand(
			"cas.showCompilationInfo",
			await this.toSourceRootPath(file),
		);
	}
	async opens(file: FileArg) {
		return commands.executeCommand(
			"cas.cmd.sendCommand",
			`ref_files --filter=[path=${await this.toSourceRootPath(file)}] --details`,
		);
	}
}
