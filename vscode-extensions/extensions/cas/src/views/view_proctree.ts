import { encodeText } from "@cas/helpers";
import { Eid, FileMode, ProcessInfo } from "@cas/types/bas.js";
import { Paged } from "@cas/types/cas_server.js";
import * as vscode from "vscode";
import { DBProvider } from "../db/index";
import { debug } from "../logger";
import { Settings } from "../settings";
import { CasTelemetryLogger, getTelemetryLoggerFor } from "../telemetry";
import { createWebviewPanel } from "../webview";
import { WorkspaceGenerator } from "../workspaces/generator";

export class CASProcTreeView {
	private panel: vscode.WebviewPanel | undefined = undefined;
	private readonly ctx: vscode.ExtensionContext;
	private dbProvider: DBProvider;
	private readonly wsGenerator: WorkspaceGenerator;
	private readonly s: Settings;
	private readonly telemetry: CasTelemetryLogger;

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
		this.telemetry = getTelemetryLoggerFor("view_proctree");
		context.subscriptions.push(
			vscode.commands.registerCommand("cas.view.procTree", (pidList) => {
				this.createNew(pidList);
			}),
		);
	}

	private sendDataToView(
		{
			func,
			id,
			eid,
		}: { func: string; id: string; eid: { pid: number; idx: number } },
		data: unknown,
	) {
		if (this.panel) {
			this.panel.webview.postMessage({ func, id, eid, data });
		}
	}

	public async createNew(
		pidList: [number, number][] | number | string | undefined = undefined,
	) {
		if (pidList !== undefined && !Array.isArray(pidList)) {
			if (typeof pidList === "string" && pidList.includes(":")) {
				pidList = [pidList.split(":").map(parseInt) as [number, number]];
			} else if (typeof pidList === "number") {
				pidList = [[pidList, 0]];
			} else {
				pidList = [];
			}
		}
		if (this.panel) {
			if (pidList) {
				this.refreshContent(pidList?.map(([pid, idx]) => ({ pid, idx })) ?? []);
			}
			this.panel.reveal(vscode.ViewColumn.One);
			return;
		} else {
			this.telemetry.logUsage("viewProctree");
			this.panel = await createWebviewPanel(
				"proctree",
				this.ctx,
				"cas.CASProcTreeView",
				"CAS Process Tree",
				vscode.ViewColumn.One,
				{
					enableScripts: true,
					retainContextWhenHidden: true,
				},
			);

			this.panel?.webview.onDidReceiveMessage(async (msg: any) => {
				switch (msg.func) {
					case "init":
						this.telemetry.logUsage("init");
						this.refreshContent(
							pidList?.map(([pid, idx]) => ({ pid, idx })) ?? [],
							msg.id,
						);
						return;
					case "getChildren":
						this.sendDataToView(
							msg,
							JSON.parse(
								(await this.dbProvider
									.getDB()
									?.runQuery(
										`children?pid=${msg.eid.pid}&idx=${msg.eid.idx}&page=${msg?.data?.page ?? 0}`,
									)) ?? "",
							),
						);
						return;
					case "getProcess":
						this.sendDataToView(
							msg,
							JSON.parse(
								(await this.dbProvider
									.getDB()
									?.runQuery(
										`proc_lookup?pid=${msg.eid.pid}&idx=${msg.eid.idx}`,
									)) ?? "",
							),
						);
						return;
					case "getProcessInfo":
						this.sendDataToView(
							msg,
							JSON.parse(
								(await this.dbProvider
									.getDB()
									?.runQuery(
										`proc?pid=${msg.eid.pid}&idx=${msg.eid.idx}&page=${msg.page ?? 0}`,
									)) ?? "",
							),
						);
						return;
					case "opn_proc_tree_win":
						this.createNew();
						return;
					case "search":
						this.telemetry.logUsage("search");
						this.sendDataToView(
							msg,
							JSON.parse(
								(await this.dbProvider
									.getDB()
									?.runQuery(`search?${new URLSearchParams(msg.data)}`)) ?? "",
							),
						);
						return;
					case "getDeps":
						this.sendDataToView(
							msg,
							JSON.parse(
								(await this.dbProvider
									.getDB()
									?.runQuery(`revdeps_of?path=${msg.data.path}`)) ?? "",
							),
						);
						return;
					case "saveJSON":
						this.telemetry.logUsage("saveJSON");
						this.sendDataToView(
							msg,
							(await this.saveProcessJSON(msg.eid))?.toJSON(),
						);
						return;
				}
			});

			this.panel?.onDidDispose(() => {
				debug("onDidDispose");
				this.panel?.dispose();
				this.panel = undefined;
			});

			this.dbProvider.onDBChange(async () => {
				debug("onDBChange");
				this.refreshContent(pidList?.map(([pid, idx]) => ({ pid, idx })) ?? []);
			});

			this.panel?.onDidChangeViewState(
				async (msg: vscode.WebviewPanelOnDidChangeViewStateEvent) => {
					debug("onDidChangeViewState" + msg);
				},
			);
		}
	}

	async saveProcessJSON(eid: Eid) {
		debug(`saving process ${eid.pid}:${eid.idx}`);
		const target = await vscode.window.showSaveDialog({
			title: `Save process ${eid.pid}:${eid.idx}`,
			filters: {
				JSON: ["json"],
			},
			saveLabel: "Save",
			defaultUri: vscode.Uri.joinPath(
				vscode.workspace.workspaceFolders?.at(0)?.uri ?? vscode.Uri.file("."),
				`process-${eid.pid}-${eid.idx}.json`,
			),
		});
		if (!target) {
			debug(`savind process ${eid.pid}:${eid.idx} cancelled`);
			return undefined;
		}
		debug(`exporting process ${eid.pid}:${eid.idx} to JSON`);
		const process = await this.dbProvider
			.getDB()
			?.getQueryResponse<ProcessInfo>(
				`proc?pid=${eid.pid}&idx=${eid.idx}&page=0`,
			);
		if (!process) {
			return undefined;
		}
		// fill in all open files if there is more than one page of them
		if (process.files_pages > 1) {
			process.openfiles.push(
				...(
					await Promise.all(
						Array.from({ length: process.files_pages - 1 }, async (_, i) => {
							const result = await this.dbProvider
								.getDB()
								?.getQueryResponse<ProcessInfo>(
									`proc?pid=${eid.pid}&idx=${eid.idx}&page=${i + 1}`,
								);
							return result?.openfiles ?? [];
						}),
					)
				).flat(),
			);
		}
		await vscode.workspace.fs.writeFile(
			target,
			encodeText(JSON.stringify(process, undefined, 2)),
		);
		debug(`saved process ${eid.pid}:${eid.idx} to ${target.fsPath}`);
		return target;
	}

	refreshContent(pidList: { pid: number; idx: number }[], id?: string) {
		debug("refreshContent");
		if (pidList.length) {
			this.sendDataToView(
				{
					func: "init",
					id: id ?? "",
					eid: pidList[0],
				},
				{
					pid_list: pidList,
				},
			);
		}
		this.dbProvider
			.getDB()
			?.getRootPid()
			.then(async (x: number) => {
				pidList = [{ pid: x, idx: 0 }];
				debug(`No pid defined root pid = ${x}`);
				this.sendDataToView(
					{
						func: "init",
						id: id ?? "",
						eid: pidList[0],
					},
					{
						pid_list: pidList,
					},
				);
			});
	}
}
