import { randomUUID } from "node:crypto";
import { decodeText } from "@cas/helpers";
import {
	type Event,
	type ExtensionContext,
	ExtensionMode,
	Uri,
	type ViewColumn,
	type Webview,
	type WebviewOptions,
	type WebviewPanel,
	type WebviewPanelOnDidChangeViewStateEvent,
	type WebviewPanelOptions,
	type WebviewView,
	window,
	workspace,
} from "vscode";
import {
	getTelemetryHost,
	isTelemetryEnabled,
	onTelemetryEnabledStateChange,
} from "./telemetry";

export async function createWebviewPanel(
	page: string,
	ctx: ExtensionContext,
	viewType: string,
	title: string,
	showOptions:
		| ViewColumn
		| {
				readonly viewColumn: ViewColumn;
				readonly preserveFocus?: boolean;
		  },
	options?: WebviewPanelOptions & WebviewOptions,
) {
	const localResourceRoots = [
		...(options?.localResourceRoots ?? []),
		Uri.joinPath(ctx.extensionUri, "dist/webview"),
	];
	const panel = window.createWebviewPanel(viewType, title, showOptions, {
		...(options ?? {}),
		enableScripts: true,
		enableCommandUris: true,
		localResourceRoots,
	});
	const sveltePanel = new SvelteWebviewPanel(panel, page, ctx);
	await sveltePanel.webview.loadHTML();
	return sveltePanel;
}

export async function loadWebviewView(
	page: string,
	ctx: ExtensionContext,
	webviewView: WebviewView,
	options?: WebviewPanelOptions & WebviewOptions,
) {
	const localResourceRoots = [
		...(options?.localResourceRoots ?? []),
		Uri.joinPath(ctx.extensionUri, "dist/webview"),
	];
	if (process.env.NODE_ENV !== "production") {
		localResourceRoots.push(Uri.parse("http://localhost:5173"));
	}
	webviewView.webview.options = {
		...webviewView.webview.options,
		...(options ?? {}),
		enableScripts: true,
		localResourceRoots,
	};
	const wrapped = new SvelteWebview(webviewView.webview, page, ctx);
	await wrapped.loadHTML();
	return wrapped;
}

export class SvelteWebview implements Webview {
	webview: Webview;
	#ctx: ExtensionContext;
	#page: string;
	get options() {
		return this.webview.options;
	}
	set options(options: WebviewOptions) {
		this.webview.options = options;
	}
	get cspSource() {
		return this.webview.cspSource;
	}

	get html() {
		return this.webview.html;
	}
	onDidReceiveMessage: Event<any>;
	postMessage(message: unknown): Thenable<boolean> {
		return this.webview.postMessage(message);
	}
	asWebviewUri(localResource: Uri): Uri {
		return this.webview.asWebviewUri(localResource);
	}

	constructor(webview: Webview, page: string, ctx: ExtensionContext) {
		this.webview = webview;
		this.#page = page;
		this.#ctx = ctx;
		this.onDidReceiveMessage = webview.onDidReceiveMessage;
		webview.onDidReceiveMessage((msg) => {
			if (msg.func === "telemetry") {
				webview.postMessage({
					...msg,
					data: { enabled: isTelemetryEnabled(), apiHost: getTelemetryHost() },
				});
			}
		});
		onTelemetryEnabledStateChange((enabled) =>
			webview.postMessage({
				func: "telemetry",
				data: { enabled, apiHost: getTelemetryHost() },
			}),
		);
	}

	async loadHTML(): Promise<void> {
		let file: string;
		let cspSource = this.cspSource;
		const nonce = randomUUID();
		if (
			this.#ctx.extensionMode === ExtensionMode.Production ||
			process.env.NODE_ENV === "production"
		) {
			file = decodeText(
				await workspace.fs.readFile(
					Uri.joinPath(
						this.#ctx.extensionUri,
						"dist",
						"webview",
						`${this.#page}.html`,
					),
				),
			);
			file = file.replaceAll("//{{webview.production}}", "");
		} else {
			const server = "http://localhost:5173";
			cspSource += ` ${server} `;
			file = await (await fetch(`${server}/${this.#page}`)).text();
			file = file
				.replaceAll(
					'<base href="{{webview.baseUri}}" />',
					`<base href="${server}/" />`,
				)
				.replaceAll("//{{webview.development}}", "");
		}
		const baseUri = this.webview.asWebviewUri(
			Uri.joinPath(this.#ctx.extensionUri, "dist", "webview"),
		);
		this.webview.html = file
			.replaceAll("{{webview.page}}", this.#page)
			.replaceAll("{{webview.cspSource}}", cspSource)
			.replaceAll("{{webview.telemetryHost}}", getTelemetryHost())
			.replaceAll("{{webview.nonce}}", nonce)
			.replaceAll(
				"{{webview.build}}",
				this.asWebviewUri(
					Uri.joinPath(this.#ctx.extensionUri, "dist", "webview"),
				).toString(),
			)
			.replaceAll("{{webview.baseUri}}", `${baseUri.toString()}/`);
	}
}

export class SvelteWebviewPanel implements WebviewPanel {
	#panel: WebviewPanel;
	#webview: SvelteWebview;
	get viewType() {
		return this.#panel.viewType;
	}

	get title() {
		return this.#panel.title;
	}
	set title(value: string) {
		this.#panel.title = value;
	}
	get iconPath():
		| Uri
		| { readonly light: Uri; readonly dark: Uri }
		| undefined {
		return this.#panel.iconPath;
	}
	set iconPath(value: Uri | { readonly light: Uri; readonly dark: Uri }) {
		this.#panel.iconPath = value;
	}
	get webview() {
		return this.#webview;
	}
	get options() {
		return this.#panel.options;
	}
	get viewColumn() {
		return this.#panel.viewColumn;
	}
	get active() {
		return this.#panel.active;
	}
	get visible() {
		return this.#panel.visible;
	}
	onDidChangeViewState: Event<WebviewPanelOnDidChangeViewStateEvent>;
	onDidDispose: Event<void>;
	reveal(viewColumn?: ViewColumn, preserveFocus?: boolean): void {
		this.#panel.reveal(viewColumn, preserveFocus);
	}
	dispose() {
		return this.#panel.dispose();
	}
	constructor(panel: WebviewPanel, page: string, ctx: ExtensionContext) {
		this.#panel = panel;
		this.#webview = new SvelteWebview(this.#panel.webview, page, ctx);
		this.onDidChangeViewState = this.#panel.onDidChangeViewState;
		this.onDidDispose = this.#panel.onDidDispose;
	}
}
