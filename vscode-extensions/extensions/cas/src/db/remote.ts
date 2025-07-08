import { withTrailingSlash } from "@cas/helpers";
import { exponentialBackoff, sleep } from "@cas/helpers/promise.js";
import { CASResult } from "@cas/types/cas_server.js";
import { ReadableStream, TextDecoderStream } from "stream/web";
import { DBInfo } from "../db/index";
import { debug } from "../logger";
import { Settings } from "../settings";
import { CASDatabase } from "./generic";

export class RemoteCASDatabase extends CASDatabase {
	serverUrl: URL;
	readonly supportsStreaming = true;
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
			const res = await fetch(this.serverUrl, {
				method: "HEAD",
				mode: "no-cors",
				signal: this.abort
					? AbortSignal.any([AbortSignal.timeout(500), this.abort])
					: AbortSignal.timeout(500),
			}).catch(() => ({
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
				ERROR: err instanceof Error ? err.message : err,
			});
		}
	}
}
