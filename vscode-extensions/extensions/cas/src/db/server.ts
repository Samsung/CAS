import { exponentialBackoff, sleep, withAbort } from "@cas/helpers";
import { CommandResult, runCommand } from "@cas/helpers/vscode/command.js";
import { CASResult } from "@cas/types/cas_server.js";
import { createServer } from "http";
import { ReadableStream } from "stream/web";
import * as v from "valibot";
import { StatusBarAlignment, StatusBarItem, window } from "vscode";
import { DBInfo } from "../db/index";
import { debug, error, srvLog, warn } from "../logger";
import { Settings } from "../settings";
import { CASDatabase } from "./generic";

const statusSchema = v.object({
	loaded_databases: v.object({
		cas: v.string(),
		ftdb: v.string(),
	}),
});

export class LocalServerCASDatabase extends CASDatabase {
	serverUrl?: URL;
	serverProcess?: CommandResult;
	serverStatus: StatusBarItem;
	readonly supportsStreaming = true;
	constructor(
		casPath: DBInfo,
		ftPath: DBInfo | undefined,
		settings: Settings,
		abort?: AbortSignal,
	) {
		super(casPath, ftPath, settings, abort);
		this.serverStatus = window.createStatusBarItem(StatusBarAlignment.Left);
		this.serverStatus.hide();
		this.settings.context.subscriptions.push(this.serverStatus);
		if (!settings.casServer) {
			window
				.showInformationMessage("CAS server command not found", "Browse")
				.then(() => {
					settings.pickCasServer();
				});
		}
	}

	isLocalDB(): boolean {
		return true;
	}

	get portRange() {
		return (
			this.settings.casServerPortRange?.split(":").map((x) => Number(x)) ?? [
				8080, 8180,
			]
		);
	}

	async checkRunning(): Promise<string> {
		let range = this.portRange;
		debug("[cas.CASDatabase] looking for existing servers");
		return Promise.any(
			Array.from(
				{ length: range[1] - range[0] + 1 },
				(_, i) => range[0] + i,
			).map(async (port) => {
				const url = `http://localhost:${port}`;
				const check = await fetch(`${url}/status`, {
					method: "HEAD",
					mode: "no-cors",
					signal: this.abort
						? AbortSignal.any([AbortSignal.timeout(1000), this.abort])
						: AbortSignal.timeout(1000),
				});
				if (check.ok) {
					const response = await fetch(`${url}/status`, {
						method: "GET",
						signal: this.abort
							? AbortSignal.any([AbortSignal.timeout(500), this.abort])
							: AbortSignal.timeout(500),
					});
					const data = v.parse(statusSchema, response.json());
					if (this.casPath && data.loaded_databases.cas !== this.casPath.path) {
						error("Server has different BAS path loaded than specified", true);
						throw new Error("Different BAS path");
					}
					if (this.ftPath && data.loaded_databases.ftdb !== this.ftPath.path) {
						error("Server has different FTDB path loaded than specified", true);
						throw new Error("Different FTDB path");
					}
					return url;
				}
				throw new Error("Server not running");
			}),
		);
	}
	isPortAvailable(port: number): Promise<boolean> {
		return new Promise<boolean>((resolve) => {
			const server = createServer();
			server
				.listen(port, () => {
					server.close();
					resolve(true);
				})
				.on("error", () => {
					resolve(false);
				});
		});
	}
	async getFirstFreePort(): Promise<number> {
		let range = this.portRange;
		for (let index = range[0]; index < range[1]; index++) {
			if (await this.isPortAvailable(index)) {
				debug(`[cas.get_first_free_port] first free ${index}`);
				return index;
			} else {
				debug(`[cas.get_first_free_port] port ${index} not free `);
			}
		}
		return -1;
	}
	async isListening(serverUrl?: string): Promise<boolean> {
		const url = serverUrl ?? this.serverUrl;
		if (!url) {
			return false;
		}
		for await (const _ of exponentialBackoff({
			initial: 25,
			base: 2,
			max: 30_000,
			signal: this.abort,
		})) {
			const res = await fetch(url, {
				method: "HEAD",
				mode: "no-cors",
				signal: this.abort
					? AbortSignal.any([AbortSignal.timeout(500), this.abort])
					: AbortSignal.timeout(500),
			}).catch(() => ({ ok: false }));
			if (res.ok) {
				return true;
			}
		}
		return false;
	}

	async start(iteration?: number): Promise<string | undefined> {
		const existing = await this.checkRunning().catch(() => undefined);
		if (existing) {
			debug(`[cas.CASDatabase] found existing server: ${existing}`);
			return existing;
		}
		debug("[cas.CASDatabase] start() No existing server found");
		// ensure casPath and casServer are defined
		if (!this.casPath || !this.settings.casServer) {
			error("CAS server isn't configured correctly!", true);
			return;
		}

		this.serverStatus.text = "$(loading~spin) CAS Server: Starting...";
		this.serverStatus.show();

		const port = await this.getFirstFreePort();
		if (port === -1) {
			error(
				`No port available in the configured range ${this.portRange[0]}-${this.portRange[1]}`,
				true,
			);
			this.serverStatus.text = "$(alert) CAS Server: No port available!";
			return;
		}
		const url = `http://localhost:${port}`;
		const args = [
			`--port=${port}`,
			"--host=localhost",
			`--casdb=${this.casPath.path}`,
			"--debug",
		];
		if (this.ftPath) {
			args.push(`--ftdb=${this.ftPath.path}`);
		}
		const {
			promise: casStarted,
			resolve: confirmStart,
			reject: rejectStart,
		} = Promise.withResolvers<void>();
		this.serverProcess = runCommand(this.settings.casServer, args, {
			onOutput(output) {
				if (output.includes("already in use")) {
					error("Port already in use");
					rejectStart("Port already in use");
				}
				if (output.includes("Start")) {
					confirmStart();
				}
				srvLog(output);
			},
		});
		srvLog(`cas.Database start ${this.settings.casServer} ${args.join(" ")}`);
		try {
			await Promise.all([
				withAbort(this.serverProcess.started, AbortSignal.timeout(2000)),
				withAbort(
					casStarted.then(() => this.isListening()),
					AbortSignal.timeout(3000),
				),
			]);
		} catch (e) {
			error(`failed starting CAS Server: ${e}`);
			setImmediate(this.serverProcess.kill);
			if ((iteration ?? 0) > 5) {
				error(`failed starting CAS Server after 5 tries: ${e}`, true);
				throw e;
			}
			await sleep(500 * 2 ** (iteration ?? 0));
			debug("attemting to start the server again");
			return this.start((iteration ?? 0) + 1);
		}
		this.serverUrl = new URL(url);
		this.setRunning?.(true);

		this.serverStatus.text = "$(check) CAS Server: Running";
		setTimeout(() => this.serverStatus.hide(), 2000);
		return;
	}

	async stop(): Promise<void> {
		this.resetStatus();
		if (this.serverProcess !== undefined) {
			debug("[cas.CASDatabase] stop() Stopping server");
			try {
				const signal = await withAbort(
					this.serverProcess.kill("SIGINT"),
					AbortSignal.timeout(250),
				).catch(() =>
					setImmediate(() => this.serverProcess?.kill("SIGKILL").catch(debug)),
				);
				debug(signal);
			} catch (e) {
				warn(`error stopping server: ${e}`);
			}
			if (
				await withAbort(
					this.serverProcess.finished,
					AbortSignal.timeout(100),
				).catch((e) =>
					debug(`server process not finished before timeout: ${e}`),
				)
			) {
				debug("[cas.CASDatabase] stop() terminated");
				this.serverProcess = undefined;
			}
			this.serverUrl = undefined;
		}
	}
	async switchFtdb(): Promise<void> {
		debug("[cas.CASDatabase] SWITCH FTDB");
		const reqUrl = `${this.serverUrl}/reload_ftdb?path=${this.ftPath?.path}&debug=true`;
		debug(`[cas.CASDatabase] url ${reqUrl}`);
		const res = await fetch(reqUrl, { signal: this.abort });
		debug(await res.text());
	}
	// runRawCmd(cmd: string, streaming: true): Promise<ReadableStream<string>>;
	// runRawCmd(cmd: string, streaming: false): Promise<string>;
	// override async runRawCmd<T extends boolean>(cmd: string, streaming: T) {
	// 	try {
	// 		const res = await fetch(new URL("raw_cmd", this.serverUrl), {
	// 			signal: this.abort,
	// 			method: "POST",
	// 			body: cmd,
	// 		});
	// 		return streaming ? (res.body as ReadableStream<string>) : res.text();
	// 	} catch (err) {
	// 		return JSON.stringify({
	// 			error: err instanceof Error ? err.message : err,
	// 		});
	// 	}
	// }

	runQuery(query: string, streaming: true): Promise<ReadableStream<string>>;
	runQuery(query: string, streaming: false): Promise<string>;
	async runQuery(query: string, streaming: boolean = false) {
		const search = new URLSearchParams(query.split("?", 2).at(1) ?? query);
		const reqUrl = new URL(query, this.serverUrl);
		reqUrl.search = search.toString();
		debug(`[cas.CASDatabase] UrlBackend.runQuery '${reqUrl}'`);
		try {
			const res = await fetch(reqUrl, { signal: this.abort });
			if (!res.ok) {
				const data = (await res.json()) as CASResult;
				if (data.ERROR) {
					throw new Error(data.ERROR);
				}
				throw new Error(res.statusText);
			}
			return streaming ? (res.body as ReadableStream<string>) : res.text();
		} catch (err) {
			return JSON.stringify({
				error: err instanceof Error ? err.message : err,
			});
		}
	}
}
