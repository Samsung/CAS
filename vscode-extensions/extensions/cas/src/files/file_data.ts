import { depsSchema } from "@cas/deps";
import { decodeText } from "@cas/helpers";
import { BASFile, FileMode, FileType } from "@cas/types/bas.js";
import { Paged } from "@cas/types/cas_server.js";
import { parse } from "valibot";
import { commands, ExtensionContext, Uri, window, workspace } from "vscode";
import type { DBProvider } from "../db";
import { debug, warn } from "../logger";
import { WorkspaceGenerator } from "../workspaces/generator";

type ExtendedFileMode = FileMode | `${FileMode}x`;
interface LimitedFileInfo {
	filename: string;
	original_path: string;
	ppid: number;
	type: FileType;
	access: ExtendedFileMode;
	link: 0 | 1;
	related?: string[];
}

export class FileData {
	deps = new Map<string, LimitedFileInfo>();
	rcm = new Map<string, string[]>();
	db: DBProvider;
	initLoadDone?: PromiseWithResolvers<void>;
	static fd: FileData;
	private constructor(context: ExtensionContext, db: DBProvider) {
		this.db = db;
		const watcher = workspace.createFileSystemWatcher("deps.json");
		watcher.onDidChange((file) => this.loadDeps(file));
		context.subscriptions.push(watcher);
	}

	public static async init(
		context: ExtensionContext,
		db: DBProvider,
	): Promise<FileData> {
		if (!this.fd) {
			this.fd = new FileData(context, db);
		}
		if (!this.fd.deps.size && !this.fd.initLoadDone) {
			this.fd.initLoadDone = Promise.withResolvers<void>();
			await workspace
				.findFiles("deps.json")
				.then((files) =>
					Promise.allSettled(files.map((file) => this.fd.loadDeps(file))),
				);
			this.fd.initLoadDone?.resolve();
		}
		await this.fd.initLoadDone?.promise;
		return this.fd;
	}

	async loadDeps(depsPath: Uri) {
		debug("[file_data] loading deps");
		const deps = parse(
			depsSchema,
			JSON.parse(decodeText(await workspace.fs.readFile(depsPath))),
		);
		// every version since v2 is supported
		if (deps.version === 1) {
			warn(
				"Workspace is using old deps.json format, some features will not be available. Updating the workspace is recommended.",
			);
			const action = await window.showWarningMessage(
				"Workspace is using old deps.json format, some features may not be available.",
				"Update workspace",
			);
			if (action === "Update workspace") {
				await commands.executeCommand("cas.ws.update");
			}
			return;
		}

		debug("[file_data] processing files");

		for (const file of deps.entries) {
			this.deps.set(file.workspace_path, {
				filename: file.filename,
				original_path: file.original_path,
				ppid: file.ppid,
				access: `${file.access}${file.binary ? "x" : ""}`,
				link: file.link,
				type: file.type,
			});
		}
		debug("[file_data] setting context for files");
		setImmediate(() => this.setContext());
	}

	async setContext() {
		const fileLists = {
			write: [],
			linked: [],
			compiled: [],
		} as Record<string, string[]>;
		for (const [path, dep] of this.deps.entries()) {
			if (dep.type === "compiled") {
				fileLists.compiled.push(path);
			}
			if (dep.access.startsWith("w")) {
				fileLists.write.push(path);
			}
			if (dep.type === "linked") {
				fileLists.linked.push(path);
			}
		}
		for (const [name, list] of Object.entries(fileLists)) {
			commands.executeCommand("setContext", `cas.files.${name}`, list);
		}
	}

	get(uri: Uri): LimitedFileInfo | undefined {
		return this.deps.get(uri.fsPath);
	}

	getString(uri: string): LimitedFileInfo | undefined {
		return this.deps.get(uri);
	}
}
