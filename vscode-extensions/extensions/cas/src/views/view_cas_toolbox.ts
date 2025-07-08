import type { CasApiEvent } from "@cas/types/webview.js";
import * as vscode from "vscode";
import type { DBProvider } from "../db/index";
import { Snippets } from "../db/snippets";
import type { Settings } from "../settings";
import { type CasTelemetryLogger, getTelemetryLoggerFor } from "../telemetry";
import type { CASCmdView } from "../views/view_cas_cmd";
import { loadWebviewView } from "../webview";
import type { WorkspaceGenerator } from "../workspaces/generator";

export class CASToolboxView implements vscode.WebviewViewProvider {
	private panel: vscode.WebviewView | undefined = undefined;
	public static readonly viewType = "cas.toolboxView";
	private readonly ctx: vscode.ExtensionContext;
	private readonly cmdView: CASCmdView;
	private readonly dbProvider: DBProvider;
	private readonly wsGenerator: WorkspaceGenerator;
	private readonly s: Settings;
	private readonly telemetry: CasTelemetryLogger;
	private snippets: Snippets;
	constructor(
		context: vscode.ExtensionContext,
		cmdView: CASCmdView,
		dbProvider: DBProvider,
		wsGenerator: WorkspaceGenerator,
		settings: Settings,
	) {
		this.ctx = context;
		this.s = settings;
		this.telemetry = getTelemetryLoggerFor("view_cas_toolbox");
		this.wsGenerator = wsGenerator;
		this.dbProvider = dbProvider;
		this.cmdView = cmdView;
		this.snippets = Snippets.init(context);
		context.subscriptions.push(
			vscode.window.registerWebviewViewProvider(CASToolboxView.viewType, this),
		);
	}

	public async resolveWebviewView(
		webviewView: vscode.WebviewView,
		_context: vscode.WebviewViewResolveContext,
		_token: vscode.CancellationToken,
	) {
		this.panel = webviewView;
		webviewView.webview.options = {
			enableScripts: true,

			localResourceRoots: [this.ctx.extensionUri],
		};

		const webview = await loadWebviewView("toolbox", this.ctx, webviewView);
		webview.onDidReceiveMessage(async (msg: CasApiEvent) => {
			switch (msg.func) {
				case "init":
					this.telemetry.logUsage("init");
					webviewView.webview.postMessage({
						...msg,
						data: {
							isGenerated: this.s.isGenerated,
							snippets: [...this.snippets.entries()],
						},
					});
					return;
				case "opn_cmd_win":
					this.telemetry.logUsage("showCommand");
					await this.cmdView.createNew();
					return;
				case "opn_ptree_win":
					this.telemetry.logUsage("showProctree");
					vscode.commands.executeCommand("cas.view.procTree");
					return;
				case "opn_og_win":
					this.telemetry.logUsage("showOpenGrok");
					vscode.commands.executeCommand("cas.view.openGrok");
					return;
				case "set_vmko":
					await this.cmdView.setVMKO();
					return;
				case "update_ws":
					this.telemetry.logUsage("updateWorkspace");
					this.wsGenerator.updateWorkspace();
					return;
				case "edit_ws":
					this.telemetry.logUsage("editWorkspace");
					this.cmdView.editWorkspace();
					return;
				case "delete_snippet":
					this.snippets.delete((msg.data as { name: string }).name);
					return;
				case "reorder_snippets":
					this.snippets.reorder(msg.data as string[]);
					return;
				case "run_cmd":
					await this.cmdView.sendCommand(msg.data as string);
					return;
				case "opn_flamegraph":
					this.telemetry.logUsage("showFlamegraph");
					await vscode.commands.executeCommand("cas.view.flamegraph");
					return;
			}
		});
		this.snippets.onchange((snippets) => {
			webview.postMessage({
				func: "snippets",
				data: {
					snippets: [...snippets.entries()],
				},
			});
		});
	}
}
