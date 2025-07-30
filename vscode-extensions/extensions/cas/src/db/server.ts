import { exponentialBackoff, sleep, withAbort } from "@cas/helpers";
import { CommandResult, runCommand } from "@cas/helpers/vscode/command.js";
import { http } from "@cas/http";
import { CASResult } from "@cas/types/cas_server.js";
import { getLogger } from "@logtape/logtape";
import { createServer } from "http";
import { ReadableStream } from "stream/web";
import * as v from "valibot";
import { StatusBarAlignment, StatusBarItem, window } from "vscode";
import { DBInfo } from "../db/index";
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
	protected readonly logger = getLogger(["CAS", "db", "server"]);
	private readonly serverLogger = getLogger(["CAS", "Server"]);
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
		this.logger.debug`Looking for existing servers`;
		return Promise.any(
			Array.from(
				{ length: range[1] - range[0] + 1 },
				(_, i) => range[0] + i,
			).map(async (port) => {
				const url = `http://localhost:${port}`;
				const check = await http.head(`${url}/status`, {
					mode: "no-cors",
					signal: this.abort
						? AbortSignal.any([AbortSignal.timeout(1000), this.abort])
						: AbortSignal.timeout(1000),
				});
				if (check.ok) {
					const response = await http.get(`${url}/status`, {
						signal: this.abort
							? AbortSignal.any([AbortSignal.timeout(500), this.abort])
							: AbortSignal.timeout(500),
					});
					const data = v.parse(statusSchema, response.json());
					if (this.casPath && data.loaded_databases.cas !== this.casPath.path) {
						this.logger
							.error`Server has different BAS path loaded than specified`;
						throw new Error("Different BAS path");
					}
					if (this.ftPath && data.loaded_databases.ftdb !== this.ftPath.path) {
						this.logger
							.error`Server has different FTDB path loaded than specified`;
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
				this.logger.debug`Found free port: ${index}`;
				return index;
			} else {
				this.logger.debug`Port ${index} is not free`;
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
			const res = await http
				.head(url, {
					mode: "no-cors",
					signal: this.abort
						? AbortSignal.any([AbortSignal.timeout(500), this.abort])
						: AbortSignal.timeout(500),
					throwHttpErrors: false,
				})
				.catch(() => ({ ok: false }));
			if (res.ok) {
				return true;
			}
		}
		return false;
	}

	async start(iteration?: number): Promise<string | undefined> {
		const existing = await this.checkRunning().catch(() => undefined);
		if (existing) {
			this.logger.debug`Found existing server: ${existing}`;
			return existing;
		}
		this.logger.debug`No existing server found`;
		// ensure casPath and casServer are defined
		if (!this.casPath || !this.settings.casServer) {
			this.logger.error`CAS server isn't configured correctly!`;
			return;
		}

		this.serverStatus.text = "$(loading~spin) CAS Server: Starting...";
		this.serverStatus.show();

		const port = await this.getFirstFreePort();
		if (port === -1) {
			this.logger
				.error`No port available in the configured range ${this.portRange[0]}-${this.portRange[1]}`;
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
			onOutput: (output) => {
				if (output.includes("already in use")) {
					this.serverLogger.error("Port already in use");
					rejectStart("Port already in use");
				}
				if (output.includes("Start")) {
					confirmStart();
				}
				this.serverLogger.debug`Server output: ${output}`;
			},
		});
		this.serverLogger
			.debug`Starting server: ${this.settings.casServer} ${args.join(" ")}`;
		try {
			await Promise.all([
				withAbort(this.serverProcess.started, AbortSignal.timeout(2000)),
				withAbort(
					casStarted.then(() => this.isListening()),
					AbortSignal.timeout(3000),
				),
			]);
		} catch (e) {
			this.logger.error`Failed starting CAS Server: ${e}`;
			setImmediate(this.serverProcess.kill);
			if ((iteration ?? 0) > 5) {
				this.logger.error`failed starting CAS Server after 5 tries: ${e}`;
				throw e;
			}
			await sleep(500 * 2 ** (iteration ?? 0));
			this.logger.debug("attemting to start the server again");
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
			this.logger.debug`Stopping server`;
			try {
				const signal = await withAbort(
					this.serverProcess.kill("SIGINT"),
					AbortSignal.timeout(250),
				).catch(() =>
					setImmediate(() =>
						this.serverProcess?.kill("SIGKILL").catch(this.serverLogger.debug),
					),
				);
				this.logger.debug`Server stopped with signal: ${signal}`;
			} catch (e) {
				this.logger.warn`Error stopping server: ${e}`;
			}
			if (
				await withAbort(
					this.serverProcess.finished,
					AbortSignal.timeout(100),
				).catch(
					(e) =>
						this.logger.debug`Server process not finished before timeout: ${e}`,
				)
			) {
				this.logger.debug`Server terminated`;
				this.serverProcess = undefined;
			}
			this.serverUrl = undefined;
		}
	}
	async switchFtdb(): Promise<void> {
		this.logger.debug`Switching FTDB`;
		const reqUrl = `${this.serverUrl}/reload_ftdb?path=${this.ftPath?.path}&debug=true`;
		this.logger.debug`Request URL: ${reqUrl}`;
		const res = await http.get(reqUrl, { signal: this.abort });
		this.logger.debug`Response: ${await res.text()}`;
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
		this.logger.debug`Running query: ${reqUrl}`;
		try {
			const res = await http.get(reqUrl, { signal: this.abort });
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
