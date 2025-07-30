import { withTrailingSlash } from "@cas/helpers";
import { exponentialBackoff } from "@cas/helpers/promise.js";
import { http } from "@cas/http";
import { CASResult } from "@cas/types/cas_server.js";
import { getLogger } from "@logtape/logtape";
import { ReadableStream } from "stream/web";
import { DBInfo } from "../db/index";
import { Settings } from "../settings";
import { CASDatabase } from "./generic";

export class RemoteCASDatabase extends CASDatabase {
	serverUrl: URL;
	readonly supportsStreaming = true;
	protected readonly logger = getLogger(["CAS", "db", "remote"]);
	constructor(
		casPath: DBInfo,
		ftPath: DBInfo | undefined,
		settings: Settings,
		abort?: AbortSignal,
	) {
		super(casPath, ftPath, settings, abort);
		this.serverUrl = new URL(withTrailingSlash(casPath.path));
	}

	isLocalDB(): boolean {
		return false;
	}
	async start(): Promise<string | undefined> {
		for await (const _ of exponentialBackoff({
			initial: 100,
			base: 2,
			max: 60_000,
			signal: this.abort,
		})) {
			const res = await http
				.head(this.serverUrl, {
					mode: "no-cors",
					signal: this.abort
						? AbortSignal.any([AbortSignal.timeout(500), this.abort])
						: AbortSignal.timeout(500),
				})
				.catch(() => ({
					ok: false,
				}));
			if (res.ok) {
				this.setRunning?.(true);
				return this.serverUrl.toString();
			}
		}
		return undefined;
	}

	async stop(): Promise<void> {
		this.resetStatus();
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
				ERROR: err instanceof Error ? err.message : err,
			});
		}
	}
}
