import { MaybePromise } from "@cas/helpers";
import { CasTelemetryLogger, getTelemetryLoggerFor } from "@cas/telemetry";
import { Eid, Process } from "@cas/types/bas.js";
import type { CasApiEvent } from "@cas/types/webview.js";
import { getLogger } from "@logtape/logtape";
import * as v from "valibot";
import { commands, ExtensionContext, ViewColumn, WebviewPanel } from "vscode";
import { DBProvider } from "../db";
import { Settings } from "../settings";
import { createWebviewPanel } from "../webview";

interface ProcessWithChildren extends Omit<Process, "children"> {
	children: number | ProcessWithChildren[];
}
interface Threads {
	count: number;
	threads: number[];
}

export class FlamegraphView {
	#panel: WebviewPanel | undefined = undefined;
	readonly #ctx: ExtensionContext;
	readonly #s: Settings;
	readonly #telemetry: CasTelemetryLogger;
	readonly #dbProvider: DBProvider;
	readonly #logger = getLogger(["CAS", "view", "flamegraph"]);
	constructor(
		context: ExtensionContext,
		settings: Settings,
		dbProvider: DBProvider,
	) {
		this.#ctx = context;
		this.#s = settings;
		this.#telemetry = getTelemetryLoggerFor("view_flamegraph");
		this.#dbProvider = dbProvider;

		context.subscriptions.push(
			commands.registerCommand("cas.view.flamegraph", () => this.createNew()),
		);
	}

	sendData<T>(msg: CasApiEvent<T>): Promise<void>;
	sendData<T>(
		msg: CasApiEvent<unknown>,
		data: T,
		panel?: WebviewPanel,
	): Promise<void>;
	async sendData<T>(
		msg: CasApiEvent<unknown>,
		data?: T,
		panel?: WebviewPanel,
	): Promise<void> {
		if (data) {
			msg = this.wrapData(msg, data);
		}
		(panel ?? this.#panel)?.webview.postMessage(msg);
	}
	private wrapData<T>(msg: CasApiEvent<unknown>, data: T): CasApiEvent<T> {
		return {
			...msg,
			data,
		};
	}

	#abortSearch = new AbortController();
	#handlers: Record<
		string,
		(msg: CasApiEvent<unknown>, panel?: WebviewPanel) => unknown
	> = {
		init: async (msg: CasApiEvent<unknown>, panel) => {
			try {
				return this.sendData(
					msg,
					{
						proc: await this.createFlameGraph(undefined, 4),
						source_root: await this.#dbProvider.getDB()?.getSourceRoot(),
					},
					panel,
				);
			} catch (_e) {
				return this.sendData(msg, {});
			}
		},
		loadChildren: checked(
			async (msg: CasApiEvent<Eid>, panel) => {
				try {
					return this.sendData(
						msg,
						await this.createFlameGraph(msg.data, 2),
						panel,
					);
				} catch (_e) {
					return this.sendData(msg, {}, panel);
				}
			},
			v.object({ pid: v.number(), idx: v.number() }),
		),
		getProcessInfo: async (msg: CasApiEvent, panel) => {
			const eid = (msg as CasApiEvent & { eid: Eid }).eid;
			return this.sendData(
				msg,
				JSON.parse(
					(await this.#dbProvider
						.getDB()
						?.runQuery(
							`proc?pid=${eid.pid}&idx=${eid.idx}&page=${(msg as CasApiEvent & { page?: number }).page ?? 0}`,
						)) ?? "",
				),
				panel,
			);
		},
		openProcTimeGraph: checked(
			async (msg: CasApiEvent<Eid>) => {
				await this.createProcTimeGraph(msg.data.pid, msg.data.idx);
			},
			v.object({ pid: v.number(), idx: v.number() }),
		),
	};

	public async createNew(): Promise<void> {
		if (this.#panel) {
			this.#panel.reveal(ViewColumn.One);
			return;
		}
		this.#telemetry.logUsage("viewFlameGraph");
		this.#panel = await createWebviewPanel(
			"flamegraph",
			this.#ctx,
			"cas.FlamegraphView",
			"Flamegraph View",
			ViewColumn.One,
			{
				enableScripts: true,
				retainContextWhenHidden: true,
				enableCommandUris: true,
			},
		);
		this.#panel.onDidDispose(() => {
			this.#telemetry.logUsage("closeFlamegraph");
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

	private async createProcTimeGraph(
		pid: string | number,
		idx: string | number,
	): Promise<void> {
		this.#telemetry.logUsage("viewProcTimeGraph");
		const panel = await createWebviewPanel(
			"flamegraph/proctime",
			this.#ctx,
			"cas.ProcTimeGraph",
			"ProcTime View",
			ViewColumn.One,
			{
				enableScripts: true,
				retainContextWhenHidden: true,
				enableCommandUris: true,
			},
		);
		panel.onDidDispose(() => {
			this.#telemetry.logUsage("closeProcTimeGraph");
			panel?.dispose();
		});
		panel.webview.onDidReceiveMessage(async (msg: CasApiEvent<unknown>) => {
			if (msg.func === "init") {
				try {
					const proc = (await this.#dbProvider
						.getDB()
						?.getProcess(pid, idx)) ?? { ERROR: "no database" };
					if ("ERROR" in proc) {
						throw new Error(proc.ERROR);
					}

					const [recusiveChildren, source_root, threads] = await Promise.all([
						this.#dbProvider.getDB()?.getChildren(pid, idx, true, false),
						this.#dbProvider.getDB()?.getSourceRoot(),
						this.#dbProvider
							.getDB()
							?.runQuery("/threads")
							.then(
								(response) =>
									(!response.includes("ERROR")
										? JSON.parse(response)
										: {
												count: 0,
												threads: [],
											}) as Threads,
							),
					]);
					(proc as ProcessWithChildren).children =
						recusiveChildren?.children ?? proc.children;
					return this.sendData(
						msg,
						{
							proc,
							source_root,
							threads,
						},
						panel,
					);
				} catch (_e) {
					return this.sendData(msg, {}, panel);
				}
			} else if (this.#handlers[msg.func]) {
				try {
					this.#handlers[msg.func](msg, panel);
				} catch (e) {
					this.sendData(
						{
							func: "error",
							id: msg.id,
							data: {
								msg,
								error: e,
							},
						},
						panel,
					);
				}
			}
		});
	}

	async createFlameGraph(eid?: Eid, depth?: number) {
		const db = this.#dbProvider.getDB();
		if (!db) {
			return;
		}
		// default to root process if not specified.
		eid ??= { pid: await db.getRootPid(), idx: 0 } as Eid;
		const rootProcess = (await db.getProcess(
			eid.pid,
			eid.idx,
		)) as ProcessWithChildren;
		if ("ERROR" in rootProcess) {
			return;
		}
		rootProcess.children = (
			await db.getChildren(eid.pid, eid.idx, true, depth ?? 3)
		).children;
		return rootProcess;
	}
}

function checkEvent<T>(
	msg: CasApiEvent<unknown>,
	validator: v.BaseSchema<unknown, T, v.BaseIssue<unknown>>,
): CasApiEvent<T> {
	return { ...msg, data: v.parse(validator, msg.data) };
}
function checked<T>(
	f: (msg: CasApiEvent<T>, panel?: WebviewPanel) => MaybePromise<unknown>,
	validator: v.BaseSchema<unknown, T, v.BaseIssue<unknown>>,
): (msg: CasApiEvent<unknown>) => unknown {
	return async (msg: CasApiEvent<unknown>, panel?: WebviewPanel) =>
		f(checkEvent(msg, validator), panel);
}
