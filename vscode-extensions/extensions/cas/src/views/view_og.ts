import { MaybePromise } from "@cas/helpers";
import type { CasApiEvent } from "@cas/types/webview.js";
import * as v from "valibot";
import {
	commands,
	ExtensionContext,
	Uri,
	ViewColumn,
	WebviewPanel,
} from "vscode";
import { OGSearchQuery, OGSearchQuerySchema, OpenGrokApi } from "../og";
import { Settings } from "../settings";
import { CasTelemetryLogger, getTelemetryLoggerFor } from "../telemetry";
import { createWebviewPanel } from "../webview";

const getLineEvent = v.object({
	path: v.string(),
	line: v.pipe(v.number(), v.integer(), v.minValue(-1)),
	projects: v.array(v.string()),
});
type GetLineEvent = v.InferOutput<typeof getLineEvent>;

export class OGView {
	#panel: WebviewPanel | undefined = undefined;
	readonly #ctx: ExtensionContext;
	readonly #s: Settings;
	readonly #telemetry: CasTelemetryLogger;
	readonly #og: OpenGrokApi;
	constructor(context: ExtensionContext, settings: Settings, og: OpenGrokApi) {
		this.#ctx = context;
		this.#s = settings;
		this.#og = og;
		this.#telemetry = getTelemetryLoggerFor("view_og_sidepanel");

		context.subscriptions.push(
			commands.registerCommand("cas.view.openGrok", () => this.createNew()),
			this.#og.urlChangeEvent((_url) => {
				if (this.#panel) {
					this.#handlers.init({
						func: "init",
						id: "",
						data: undefined,
					});
				}
			}),
		);
	}

	sendData<T>(msg: CasApiEvent<T>): Promise<void>;
	sendData<T>(msg: CasApiEvent<unknown>, data: T): Promise<void>;
	async sendData<T>(msg: CasApiEvent<unknown>, data?: T): Promise<void> {
		if (data) {
			msg = this.wrapData(msg, data);
		}
		this.#panel?.webview.postMessage(msg);
	}
	private wrapData<T>(msg: CasApiEvent<unknown>, data: T): CasApiEvent<T> {
		return {
			...msg,
			data,
		};
	}

	#abortSearch = new AbortController();
	#handlers: Record<string, (msg: CasApiEvent<unknown>) => unknown> = {
		init: async (msg: CasApiEvent<unknown>) => {
			try {
				return this.sendData(msg, {
					projects: await this.#og.getProjects(),
					enabled: this.#og.url.protocol !== "error:",
				});
			} catch (_e) {
				return this.sendData(msg, {
					projects: [],
					enabled: false,
				});
			}
		},
		search: checked(async (msg: CasApiEvent<OGSearchQuery>) => {
			this.#abortSearch.abort();
			this.#abortSearch = new AbortController();
			return this.sendData(
				msg,
				await this.#og.search(msg.data, this.#abortSearch.signal),
			);
		}, OGSearchQuerySchema),
		file: checked(async (msg: CasApiEvent<string>) => {
			return this.sendData(msg, await this.#og.getFile(msg.data));
		}, v.string()),
		openLine: checked(async (msg: CasApiEvent<GetLineEvent>) => {
			const uri = Uri.parse(
				`opengrok://${msg.data.path}#${msg.data.projects.join(",")}`,
				true,
			);
			return this.#og.openFileInEditor(uri, msg.data.line);
		}, getLineEvent),
	};

	public async createNew(): Promise<void> {
		if (this.#panel) {
			this.#panel.reveal(ViewColumn.One);
			return;
		}
		this.#telemetry.logUsage("viewOpenGrok");
		this.#panel = await createWebviewPanel(
			"og",
			this.#ctx,
			"cas.OGView",
			"OpenGrok Search",
			ViewColumn.One,
			{
				enableScripts: true,
				retainContextWhenHidden: true,
				enableCommandUris: true,
			},
		);
		this.#panel.onDidDispose(() => {
			this.#telemetry.logUsage("closeOpenGrok");
			this.#panel?.dispose();
			this.#panel = undefined;
		});
		this.#panel.webview.onDidReceiveMessage((msg: CasApiEvent<unknown>) => {
			if (this.#handlers[msg.func]) {
				try {
					this.#handlers[msg.func](msg);
				} catch (e) {
					this.sendData({
						func: "error",
						id: msg.id,
						data: {
							msg,
							error: e,
						},
					});
				}
			}
		});
	}
}

function checkEvent<T>(
	msg: CasApiEvent<unknown>,
	validator: v.BaseSchema<unknown, T, v.BaseIssue<unknown>>,
): CasApiEvent<T> {
	return { ...msg, data: v.parse(validator, msg.data) };
}
function checked<T>(
	f: (msg: CasApiEvent<T>) => MaybePromise<unknown>,
	validator: v.BaseSchema<unknown, T, v.BaseIssue<unknown>>,
): (msg: CasApiEvent<unknown>) => unknown {
	return async (msg: CasApiEvent<unknown>) => f(checkEvent(msg, validator));
}
