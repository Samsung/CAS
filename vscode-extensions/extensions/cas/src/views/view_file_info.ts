import type { CasApiEvent } from "@cas/types/webview.js";
import * as vscode from "vscode";
import type { DBProvider } from "../db/index";
import { Snippets } from "../db/snippets";
import { FileData } from "../files/file_data";
import type { Settings } from "../settings";
import { type CasTelemetryLogger, getTelemetryLoggerFor } from "../telemetry";
import type { CASCmdView } from "../views/view_cas_cmd";
import { loadWebviewView } from "../webview";
import type { WorkspaceGenerator } from "../workspaces/generator";

export class CASFileInfoView implements vscode.WebviewViewProvider {
	private panel: vscode.WebviewView | undefined = undefined;
	public static readonly viewType = "cas.fileInfo";
	private readonly ctx: vscode.ExtensionContext;
	private readonly cmdView: CASCmdView;
	private readonly dbProvider: DBProvider;
	private readonly wsGenerator: WorkspaceGenerator;
	private readonly s: Settings;
	private readonly telemetry: CasTelemetryLogger;
	private snippets: Snippets;
	private readonly filedata: Promise<FileData>;
	constructor(
		context: vscode.ExtensionContext,
		cmdView: CASCmdView,
		dbProvider: DBProvider,
		wsGenerator: WorkspaceGenerator,
		settings: Settings,
	) {
		this.ctx = context;
		this.s = settings;
		this.telemetry = getTelemetryLoggerFor("view_file_info");
		this.wsGenerator = wsGenerator;
		this.dbProvider = dbProvider;
		this.cmdView = cmdView;
		this.snippets = Snippets.init(context);
		context.subscriptions.push(
			vscode.window.registerWebviewViewProvider(CASFileInfoView.viewType, this),
		);
		this.filedata = FileData.init(context, this.dbProvider);
		context.subscriptions.push(
			vscode.window.onDidChangeActiveTextEditor(async (editor) => {
				if (
					(await this.filedata).get(editor?.document.uri ?? vscode.Uri.file(""))
				) {
					vscode.commands.executeCommand("setContext", "casFile", true);
				} else {
					vscode.commands.executeCommand("setContext", "casFile", false);
				}
			}),
		);
	}

	async getFileData(uri: vscode.Uri) {
		return {
			fileData: (await this.filedata).get(uri),
			translationUnits: {},
		};
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

		const webview = await loadWebviewView("fileInfo", this.ctx, webviewView);
		webview.onDidReceiveMessage(async (message: CasApiEvent) => {
			if (message.func === "init") {
				webview.postMessage({
					func: "init",
					id: message.id,
					data: vscode.window.activeTextEditor?.document.uri
						? await this.getFileData(
								vscode.window.activeTextEditor?.document.uri,
							)
						: {},
				});
			}
		});
		vscode.window.onDidChangeActiveTextEditor(async (editor) => {
			webview.postMessage({
				func: "init",
				data: editor?.document.uri
					? await this.getFileData(editor?.document.uri)
					: {},
			});
		});
	}
}
