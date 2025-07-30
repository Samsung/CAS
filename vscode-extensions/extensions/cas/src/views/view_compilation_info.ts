import { CasTelemetryLogger, getTelemetryLoggerFor } from "@cas/telemetry";
import { Paged } from "@cas/types/cas_server.js";
import { getLogger } from "@logtape/logtape";
import * as vscode from "vscode";
import { DBProvider } from "../db/index";
import { Settings } from "../settings";
import { createWebviewPanel } from "../webview";

export class CASCompilationInfo {
	private panel: vscode.WebviewPanel | undefined = undefined;
	private readonly ctx: vscode.ExtensionContext;
	private dbProvider: DBProvider;
	private readonly s: Settings;
	private readonly telemetry: CasTelemetryLogger;
	private readonly logger = getLogger(["CAS", "view", "compilation_info"]);
	constructor(
		context: vscode.ExtensionContext,
		dbProvider: DBProvider,
		settings: Settings,
	) {
		this.ctx = context;
		this.s = settings;
		this.dbProvider = dbProvider;
		this.telemetry = getTelemetryLoggerFor("view_cas_cmd");
		context.subscriptions.push(
			vscode.commands.registerCommand(
				"cas.showCompilationInfo",
				async (path?: string) => {
					await this.createNew(path);
				},
			),
		);
	}

	private sendDataToView(
		{ func, id }: { func: string; id?: string },
		data?: unknown,
	) {
		this.panel?.webview.postMessage({ func, id, data });
	}

	public async createNew(path?: string) {
		if (this.panel) {
			this.panel.reveal(vscode.ViewColumn.One);
			if (path) {
				this.sendDataToView(
					{ func: "init" },
					{
						info: (
							await this.dbProvider
								.getDB()
								?.runCmd<Paged<{}>>(`compilation_info_for --path=${path}`, true)
						)?.entries.at(0),
					},
				);
			}
			return;
		} else {
			this.panel = await createWebviewPanel(
				"compInfo",
				this.ctx,
				"cas.compInfo",
				"CAS Compilation Info",
				vscode.ViewColumn.One,
				{
					enableScripts: true,
					retainContextWhenHidden: true,
				},
			);
			let initDone: (v?: unknown) => unknown;
			const initialized = new Promise((resolve) => (initDone = resolve));
			this.panel.onDidDispose(() => {
				this.logger.debug`Webview disposed`;
				this.panel?.dispose();
				this.panel = undefined;
			});

			this.panel.webview.onDidReceiveMessage(async (msg) => {
				initDone();
				if (msg.func === "init" && path) {
					this.sendDataToView(
						{ ...msg },
						{
							info: (
								await this.dbProvider
									.getDB()
									?.runCmd<Paged<{}>>(
										`compilation_info_for --path=${path}`,
										true,
									)
							)?.entries.at(0),
						},
					);
				}
			});

			this.panel.onDidChangeViewState(
				async (msg: vscode.WebviewPanelOnDidChangeViewStateEvent) => {
					this.logger.debug`Webview view state changed: ${msg}`;
				},
			);
			await initialized;
		}
	}
}
