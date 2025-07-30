import { exponentialBackoff } from "@cas/helpers";
import { CasTelemetryLogger, getTelemetryLoggerFor } from "@cas/telemetry";
import { getLogger } from "@logtape/logtape";
import { existsSync, readFileSync } from "fs";
import { basename, join } from "path";
import * as vscode from "vscode";
import { DBProvider } from "../db/index";
import { internalSnippets } from "../db/snippets";
import { Settings } from "../settings";
import { WorkspaceGenerator } from "../workspaces/generator";
import { ManifestSettings } from "../workspaces/manifest";

export class DepsTree implements vscode.TreeDataProvider<Dependency> {
	private readonly view: vscode.TreeView<Dependency>;
	private readonly dbProvider: DBProvider;
	private readonly wsGenerator: WorkspaceGenerator;
	private readonly decorationProvider: DepsDecorationProvider;
	modules: string[] = [];
	private readonly s: Settings;
	private readonly telemetry: CasTelemetryLogger;
	private readonly ws: ManifestSettings;
	private readonly logger = getLogger(["CAS", "view", "depstree"]);

	private linkedModulesPaths: string[] = [];

	constructor(
		context: vscode.ExtensionContext,
		dbProvider: DBProvider,
		wsGenerator: WorkspaceGenerator,
		settings: Settings,
		manifestSettings: ManifestSettings,
	) {
		this.s = settings;
		this.ws = manifestSettings;
		this.dbProvider = dbProvider;
		this.wsGenerator = wsGenerator;
		this.telemetry = getTelemetryLoggerFor("view_depstree");

		this.updateModules();
		this.decorationProvider = new DepsDecorationProvider(this);
		vscode.window.registerFileDecorationProvider(this.decorationProvider);
		this.view = vscode.window.createTreeView("cas.depsTree", {
			treeDataProvider: this,
			canSelectMany: true,
		});
		context.subscriptions.push(this.view);

		context.subscriptions.push(
			vscode.commands.registerCommand("cas.depsTree.refreshEntry", () => {
				this.refresh();
			}),
		);
		vscode.commands.registerCommand(
			"cas.depsTree.ws.generate",
			(selected, items) => {
				this.genWorkspace(selected, items);
			},
		);
		const projectName = this.s.projName;
		if (projectName) {
			vscode.commands.registerCommand(
				"cas.depsTree.ws.add",
				(selected, items) => {
					this.addToWorkspace(selected, items);
				},
			);
			vscode.commands.registerCommand(
				"cas.depsTree.ws.remove",
				(selected, items) => {
					this.removeFromWorkspace(selected, items);
				},
			);
		}

		vscode.commands.registerCommand("cas.depsTree.refresh", () => {
			this.refresh();
		});
	}

	private updateModules() {
		let modules: string[] = [];
		let modFile = join(Settings.getWorkspaceDir(), "modules.json");
		if (existsSync(modFile)) {
			modules = JSON.parse(readFileSync(modFile, "utf-8"));
			this.logger.debug((l) => l`Loaded modules: ${JSON.stringify(modules)}`);
		}
		this.modules = modules;
	}

	public async addToWorkspace(
		selected: Dependency | vscode.Uri,
		items: Dependency[],
	) {
		const projectName = this.s.projName;
		if (projectName) {
			let paths: string[];
			if (items && items.length > 1) {
				paths = items.map((x) => x.path);
			} else if (
				selected instanceof vscode.Uri &&
				selected.scheme === "opengrok"
			) {
				const sourceRoot = await this.dbProvider.getDB()?.getSourceRoot();
				paths = [
					join(
						sourceRoot ?? "/",
						selected.path.slice(selected.path.indexOf("/", 1)),
					),
				];
			} else {
				paths = [selected.path];
			}
			paths = paths.concat(this.modules);
			let cmd = `deps --path=${paths.join(":")} --cached ${internalSnippets.get("depsFilter")}`;
			await this.wsGenerator.updateWorkspace(cmd, paths);
			this.refresh();
		}
	}

	public async removeFromWorkspace(selected: Dependency, items: Dependency[]) {
		const projectName = this.s.projName;
		if (projectName) {
			let paths: string[];
			if (items && items.length > 1) {
				paths = items.map((x) => x.path);
			} else {
				paths = [selected.path];
			}

			let mods = this.modules.filter(function (el) {
				return !paths.includes(el);
			});
			let cmd = `deps --path=${mods.join(":")} --cached ${internalSnippets.get("depsFilter")}`;
			await this.wsGenerator.updateWorkspace(cmd, mods);
			this.refresh();
		}
	}

	public async genWorkspace(selected: Dependency, items: Dependency[]) {
		this.telemetry.logUsage("generateWorkspace");
		let projectName: string | undefined = undefined;
		let paths: string[];
		if (items && items.length > 1) {
			projectName = await this.wsGenerator.getInputBox();
			paths = items.map((x) => x.path);
		} else {
			projectName = basename(selected.path);
			paths = [selected.path];
		}
		if (projectName) {
			let cmd = `deps --path=${paths.join(":")} --cached ${internalSnippets.get("depsFilter")}`;
			this.wsGenerator.generateWorkspace(cmd, projectName, paths);
		}
	}

	getTreeItem(element: Dependency): vscode.TreeItem {
		return element;
	}

	getChildren(element?: Dependency): Thenable<Dependency[]> {
		if (element) {
			return this.getDependency(element.path);
		} else {
			return this.getLinkedModules();
		}
	}

	private getState(path: string): vscode.TreeItemCollapsibleState {
		return this.linkedModulesPaths.indexOf(path) !== -1
			? vscode.TreeItemCollapsibleState.Collapsed
			: vscode.TreeItemCollapsibleState.None;
	}

	private async getLinkedModules(): Promise<Dependency[]> {
		for await (const _ of exponentialBackoff({
			initial: 50,
			base: 4,
			condition: (i) => i < 3,
		})) {
			const ret: string[] | undefined = await this.dbProvider
				.getDB()
				?.getLinkedModules();
			if (ret) {
				this.linkedModulesPaths = ret;
				return this.linkedModulesPaths.map((x) => {
					return new Dependency(
						basename(x),
						x,
						this.getState(x),
						this.ws.isLocal,
					);
				});
			}
		}
		this.logger.error`No linked modules found`;
		return [];
	}

	private async getDependency(file: string): Promise<Dependency[]> {
		let deps: string[] | undefined = await this.dbProvider
			.getDB()
			?.getModDeps(file);
		if (deps) {
			return deps.map((x) => {
				return new Dependency(
					basename(x),
					x,
					this.getState(x),
					this.ws.isLocal,
				);
			});
		} else {
			return [];
		}
	}

	private _onDidChangeTreeData: vscode.EventEmitter<
		Dependency | undefined | void
	> = new vscode.EventEmitter<Dependency | undefined | void>();
	readonly onDidChangeTreeData: vscode.Event<Dependency | undefined | void> =
		this._onDidChangeTreeData.event;

	refresh(): void {
		this.telemetry.logUsage("refresh");
		let lastMods = this.modules.map((x) => x);
		this.updateModules();
		let diffs = lastMods.concat(this.modules.map((x) => x));
		this.decorationProvider.update(diffs.map((x) => vscode.Uri.parse(x)));
		this._onDidChangeTreeData.fire();
	}
}

export class Dependency extends vscode.TreeItem {
	constructor(
		public readonly name: string,
		public readonly path: string,
		public readonly collapsibleState: vscode.TreeItemCollapsibleState,
		private readonly isLocal: boolean,
	) {
		super(name, collapsibleState);
		this.tooltip = `${this.path}`;
		this.resourceUri = vscode.Uri.parse(this.path);
		this.description = !this.isLocal || existsSync(this.path) ? "" : "removed";
	}
}

class DepsDecorationProvider implements vscode.FileDecorationProvider {
	private readonly tree: DepsTree;
	constructor(tree: DepsTree) {
		this.tree = tree;
	}
	private readonly _onDidChangeFileDecorations: vscode.EventEmitter<
		vscode.Uri | vscode.Uri[]
	> = new vscode.EventEmitter<vscode.Uri | vscode.Uri[]>();
	readonly onDidChangeFileDecorations: vscode.Event<vscode.Uri | vscode.Uri[]> =
		this._onDidChangeFileDecorations.event;

	update(uri: vscode.Uri[]) {
		if (uri.length && uri.some((x) => x instanceof vscode.Uri)) {
			this._onDidChangeFileDecorations.fire(uri);
		}
	}
	provideFileDecoration(
		uri: vscode.Uri,
		_token: vscode.CancellationToken,
	): vscode.ProviderResult<vscode.FileDecoration> {
		if (this.tree.modules.indexOf(uri.fsPath) !== -1) {
			return {
				color: new vscode.ThemeColor("charts.green"),
				badge: "WS",
				tooltip: "Workspace file",
			};
		}
		return {
			color: new vscode.ThemeColor("charts.foreground"),
		};
	}
}
