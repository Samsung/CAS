import { History } from "@cas/helpers/history.js";
import type { historyEntry } from "@cas/webview/src/lib/cas/types";
import { basename } from "path";
import { scheduler } from "timers/promises";
import { isDeepStrictEqual } from "util";
import * as vscode from "vscode";
import { DBProvider } from "../db/index";
import { internalSnippets, Snippets } from "../db/snippets";
import { debug } from "../logger";
import { Settings } from "../settings";
import { CasTelemetryLogger, getTelemetryLoggerFor } from "../telemetry";
import { createWebviewPanel } from "../webview";
import { WorkspaceGenerator } from "../workspaces/generator";

export class CASCmdView {
	private panel: vscode.WebviewPanel | undefined = undefined;
	private cmdHistory: History<historyEntry>;
	private readonly ctx: vscode.ExtensionContext;
	private dbProvider: DBProvider;
	private readonly wsGenerator: WorkspaceGenerator;
	private readonly s: Settings;
	private readonly telemetry: CasTelemetryLogger;
	private readonly snippets: Snippets;
	constructor(
		context: vscode.ExtensionContext,
		dbProvider: DBProvider,
		wsGenerator: WorkspaceGenerator,
		settings: Settings,
	) {
		this.ctx = context;
		this.s = settings;
		this.dbProvider = dbProvider;
		this.wsGenerator = wsGenerator;
		this.telemetry = getTelemetryLoggerFor("view_cas_cmd");
		this.snippets = Snippets.init(context);
		context.subscriptions.push(
			vscode.commands.registerCommand("cas.view.command", async () => {
				await this.createNew();
			}),
		);
		context.subscriptions.push(
			vscode.commands.registerCommand("cas.process.ref", async (selection) => {
				await this.createNew();
				this.getProcref(selection);
			}),
		);
		context.subscriptions.push(
			vscode.commands.registerCommand("cas.history.back", () => {
				this.cmdHistBack();
			}),
		);
		context.subscriptions.push(
			vscode.commands.registerCommand("cas.history.forward", () => {
				this.cmdHistForward();
			}),
		);
		context.subscriptions.push(
			vscode.commands.registerCommand("cas.ws.generate.selection", () => {
				this.genWSFromSelection();
			}),
		);
		context.subscriptions.push(
			vscode.commands.registerCommand("cas.ws.edit", () => {
				this.editWorkspace();
			}),
		);
		context.subscriptions.push(
			vscode.commands.registerCommand("cas.cmd.sendCommand", (cmd: string) =>
				this.sendCommand(cmd),
			),
		);
		this.cmdHistory = new History<historyEntry>(
			this.ctx?.workspaceState.get("cas.cmd.history") ?? [],
		);
	}

	private _getSelection(selected: any): string | undefined {
		if (selected instanceof vscode.Uri) {
			return selected.fsPath;
		} else if (selected instanceof Object && selected.v) {
			return selected.v;
		} else {
			return undefined;
		}
	}

	private sendDataToView(
		{ func, id }: { func: string; id?: string },
		data?: unknown,
	) {
		this.panel?.webview.postMessage({ func, id, data });
	}

	public genWSFromSelection() {
		if (this.panel) {
			this.sendDataToView({ func: "gatherSelected" });
		}
	}

	public async sendCommand(
		cmd: string,
		page?: number,
		entries?: number,
		saveHistory?: boolean,
	) {
		if (this.panel === undefined) {
			await this.createNew();
		}
		if (this.panel) {
			this.telemetry.logUsage("setCmdInput", { command: cmd });
			this.sendDataToView(
				{ func: "setCmdInput" },
				{ cmd, page, entries, saveHistory },
			);
		}
	}

	public async setVMKO() {
		await this.sendCommand(
			`lm --filter=[path=*vmlinux,type=wc]or[path=*.ko,type=wc] deps --cached ${internalSnippets.get("depsFilter")}`,
		);
	}

	public async editWorkspace() {
		if (this.panel === undefined) {
			await this.createNew();
		}
		if (this.panel) {
			let genCmd = this.s.genCmd;
			if (genCmd !== undefined) {
				this.sendDataToView({ func: "setCmdInput" }, { cmd: genCmd });
			} else {
				vscode.window.showErrorMessage(
					`ERROR - workspace file have no cas.gen_cmd setting.`,
				);
			}
		}
	}

	public getDeps(selected: any) {
		this.telemetry.logUsage("getDeps");
		let input = this._getSelection(selected);
		if (input) {
			this.sendDataToView(
				{ func: "setCmdInput" },
				{ cmd: `deps_for --path=${input} --details` },
			);
		}
	}

	public getProcref(selected: any) {
		this.telemetry.logUsage("getProcref");
		let input = this._getSelection(selected);
		if (input) {
			if (typeof input === "string") {
				input = input.split(":")[0];
			}
			this.sendDataToView(
				{ func: "setCmdInput" },
				{ cmd: `procref --pid=${input} --details` },
			);
		}
	}

	public getOpens(selected: any) {
		this.telemetry.logUsage("getOpens");
		let input = this._getSelection(selected);
		if (input) {
			this.sendDataToView(
				{ func: "setCmdInput" },
				{ cmd: `ref_files --filter=[path=${input}] --details` },
			);
		}
	}

	public getFaccess(selected: any) {
		let input = this._getSelection(selected);
		if (input) {
			this.sendDataToView(
				{ func: "setCmdInput" },
				{ cmd: `faccess --path=${input} --details` },
			);
		}
	}

	public getCompInfo(selected: any) {
		let input = this._getSelection(selected);
		if (input) {
			this.sendDataToView(
				{ func: "setCmdInput" },
				{ cmd: `ci --path=${input}` },
			);
		}
	}

	public async genWorkspaceCmd(cmd: string) {
		this.telemetry.logUsage("generateWorkspace", {
			update: false,
			workspace_source: "cmd",
		});
		let projectName: string | undefined = await this.wsGenerator.getInputBox();
		if (projectName) {
			this.wsGenerator.generateWorkspace(cmd, projectName);
		} else {
			vscode.window.showWarningMessage("Workspace generation was canceled");
		}
	}

	public async updateWorkspaceFromCmd(cmd: string) {
		this.telemetry.logUsage("generateWorkspace", {
			update: true,
			workspace_source: "cmd",
		});
		this.wsGenerator.updateWorkspace(cmd);
	}

	public async genWorkspaceFromPaths(paths: string[]) {
		this.telemetry.logUsage("generateWorkspace", {
			update: false,
			workspace_source: "paths",
		});
		let projectName: string | undefined = undefined;
		if (paths && paths.length > 0) {
			if (paths.length > 1) {
				projectName = await this.wsGenerator.getInputBox();
			} else {
				projectName = basename(paths[0]);
			}

			if (projectName) {
				let cmd = `deps --path=${paths.join(":")} --cached ${internalSnippets.get("depsFilter")}`;
				this.wsGenerator.generateWorkspace(cmd, projectName, paths);
			} else {
				vscode.window.showWarningMessage("Workspace generation was canceled");
			}
		} else {
			vscode.window.showErrorMessage("Empty selection!");
		}
	}

	public async createNew() {
		if (this.panel) {
			this.panel.reveal(vscode.ViewColumn.One);
			return;
		} else {
			this.panel = await createWebviewPanel(
				"cmd",
				this.ctx,
				"cas.CASCmdView",
				"CAS Database Client",
				vscode.ViewColumn.One,
				{
					enableScripts: true,
					retainContextWhenHidden: true,
				},
			);
			let initDone: (v?: unknown) => unknown;
			const initialized = new Promise((resolve) => (initDone = resolve));
			this.panel.webview.onDidReceiveMessage(async (msg: any) => {
				switch (msg.func) {
					case "init":
						this.telemetry.logUsage("init");
						initDone();
						return this.sendDataToView(msg, {
							isGenerated: this.s.isGenerated,
							history: this.cmdHistory,
							enabled: this.dbProvider.currentDb?.casPath.path !== undefined,
						});
					case "run_cmd":
						this.runCmd(
							msg.cmd,
							msg.page,
							msg.entries_num,
							!msg.saveHistory,
							msg.id,
						);
						return;
					case "hist": {
						this.telemetry.logUsage("hist", { direction: msg.action });

						switch (msg.action) {
							case "back":
								this.cmdHistBack();
								break;
							case "forward":
								this.cmdHistForward();
								break;
						}
						return;
					}
					case "selected_items":
						this.genWorkspaceFromPaths(msg.selections);
						return;
					case "gen_ws_cmd":
						this.genWorkspaceCmd(msg.cmd);
						return;
					case "update_ws_cmd":
						this.updateWorkspaceFromCmd(msg.cmd);
						return;
					case "opn_cmd_win":
						await this.createNew();
						return;
					case "create_snippet":
						this.snippets.set(msg.name, msg.value);
						return;
				}
			});

			this.panel.onDidDispose(() => {
				debug("onDidDispose");
				this.panel?.dispose();
				this.panel = undefined;
			});

			this.panel.onDidChangeViewState(
				async (msg: vscode.WebviewPanelOnDidChangeViewStateEvent) => {
					debug("onDidChangeViewState" + msg);
				},
			);
			await initialized;
		}
	}

	public cmdHistBack() {
		const historyCmd = this.cmdHistory.undo();
		if (historyCmd) {
			this.sendCommand(historyCmd[0], historyCmd[1], historyCmd[2], false);
		}
	}

	public cmdHistForward() {
		const historyCmd = this.cmdHistory.redo();
		if (historyCmd) {
			this.sendCommand(historyCmd[0], historyCmd[1], historyCmd[2], false);
		}
	}

	private async runCmd(
		cmd: string,
		page: number = 0,
		entries: number | null = 100,
		skipHist = false,
		id: string = "",
	) {
		await this.createNew();
		if (this.panel) {
			if (!skipHist) {
				const shouldAdd = !isDeepStrictEqual(this.cmdHistory.current(), [
					cmd,
					page,
					entries,
				]);
				if (shouldAdd) {
					this.cmdHistory.push([cmd, page, entries ?? 0]);
					setImmediate(this.saveHistory);
				}
			}
			const data = await this.dbProvider
				.getDB()
				?.getResponse(
					cmd + " --page=" + page + " --entries-per-page=" + (entries ?? 0),
				);
			this.sendDataToView({ func: "run_cmd", id }, data);
		}
	}

	saveHistory() {
		this.ctx?.workspaceState.update(
			"cas.cmd.history",
			this.cmdHistory.internalArray,
		);
	}
}
