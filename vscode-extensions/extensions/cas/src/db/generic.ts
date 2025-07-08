import { parseStream } from "@cas/helpers/json.js";
import { LinkedModulePath, Process } from "@cas/types/bas.js";
import {
	CASChildrenResult,
	CASProcessInfoResponse,
	CASProcessResponse,
	CASResult,
	CASSuccessResults,
	Paged,
} from "@cas/types/cas_server.js";
import QuickLRU from "quick-lru";
import { ReadableStream } from "stream/web";
import { chain } from "stream-chain";
import { Parser, parser } from "stream-json";
import { connectTo as asmConnectTo } from "stream-json/Assembler";
import { commands, Disposable } from "vscode";
import { DBInfo } from "../db/index";
import { debug } from "../logger";
import { Settings } from "../settings";
export abstract class CASDatabase implements Disposable {
	casPath: DBInfo;
	ftPath?: DBInfo;
	abort?: AbortSignal;
	running: Promise<boolean>;
	setRunning?: (value: boolean | PromiseLike<boolean>) => void;
	settings: Settings;
	cache = new QuickLRU<string, string>({ maxSize: 10 });

	readonly supportsStreaming: boolean = false;
	//#region constructor and utils
	constructor(
		casPath: DBInfo,
		ftPath: DBInfo | undefined,
		settings: Settings,
		abort?: AbortSignal,
	) {
		this.casPath = casPath;
		this.ftPath = ftPath;
		this.settings = settings;
		this.running = this.resetStatus();
		this.abort = abort;
		this.start();
	}
	resetStatus() {
		commands.executeCommand("setContext", "cas.serverRunning", false);
		this.running = new Promise((resolve) => {
			this.setRunning = resolve;
			commands.executeCommand("setContext", "cas.serverRunning", true);
		});
		return this.running;
	}
	async dispose(): Promise<void> {
		await this.stop();
	}
	//#endregion
	//#region abstract
	abstract isLocalDB(): boolean;
	abstract start(): Promise<string | undefined>;
	abstract stop(): Promise<void>;
	abstract switchFtdb(): Promise<void>;
	/**
	 * Run a aribtrary CAS server query
	 * @param query path with arguments - e.g. `raw_cmd?cmd=lm`
	 * @param streaming
	 */
	abstract runQuery(
		query: string,
		streaming: true,
	): Promise<ReadableStream<string>>;
	abstract runQuery(query: string, streaming?: false): Promise<string>;
	abstract runQuery<T extends boolean>(
		query: string,
		streaming?: T,
	): Promise<T extends false ? string : ReadableStream<string>>;
	//#endregion
	//#region generic implementations
	/**
	 * Allows running a command to have different behavior to queries - e.g. using a POST request
	 *
	 * Optionally overriden by implementations
	 * @param cmd
	 * @param streaming
	 * @returns
	 */
	async runRawCmd<T extends boolean>(
		cmd: string,
		streaming: T,
	): Promise<T extends false ? string : ReadableStream<string>> {
		return this.runQuery(`raw_cmd?cmd=${encodeURI(cmd)}`, streaming);
	}

	/**
	 * wrapper for running a CAS client command, as it's the most commonly used option.
	 *
	 * Not intended to be overriden by implementations
	 * @param cmd
	 * @param deserialize
	 * @param cache
	 */
	runCmd(cmd: string, deserialize?: false, cache?: boolean): Promise<string>;
	runCmd<T>(cmd: string, deserialize?: true, cache?: boolean): Promise<T>;
	async runCmd<T>(
		cmd: string,
		deserialize = false,
		cache = false,
	): Promise<string | T> {
		if (cache && this.cache.has(cmd)) {
			return deserialize
				? JSON.parse(this.cache.get(cmd)!)
				: this.cache.get(cmd)!;
		}
		await this.running;
		debug(
			`[cas.CASDatabase] runCmd '${cmd.length > 1000 ? cmd.slice(0, 1000) + "..." : cmd}'`,
		);
		const result = await this.runRawCmd(
			cmd,
			this.supportsStreaming && deserialize,
		);
		if (deserialize && this.supportsStreaming && typeof result !== "string") {
			const parsed = await parseStream<T>(result);
			if (
				cache &&
				!(parsed as CASResult).ERROR &&
				"entries" in (parsed as CASResult) &&
				(parsed as CASSuccessResults<T>).entries?.length < 10_000
			) {
				// unfortunately, since we consumed the stream we need to restringify
				this.cache.set(cmd, JSON.stringify(parsed));
			}
			return parsed;
		}
		if (
			cache &&
			(result as string).length < 2 * 1024 * 1024 &&
			!(result as string)
				.substring(0, 64)
				.match(/\{\s*["']ERROR["']:(?!\s*undefined|\s*null).+/i)
		) {
			this.cache.set(cmd, result as string);
		}
		return deserialize ? JSON.parse(result as string) : (result as string);
	}

	async getQueryResponse<T>(query: string): Promise<T> {
		await this.running;
		debug(`[cas.CASDatabase] getQueryResponse '${query}'`);
		const result = await this.runQuery(query, this.supportsStreaming);
		if (this.supportsStreaming) {
			const pipeline = chain([result, parser()] as
				| readonly [never, Parser]
				| readonly [ReadableStream<string>, Parser]);
			const asm = asmConnectTo(pipeline);
			return new Promise<T>((resolve) =>
				asm.on("done", (asm) => resolve(asm.current)),
			);
		}
		return JSON.parse(result as string);
	}
	async getResponse<T>(cmd: string, cache = false): Promise<T> {
		return this.runCmd(cmd, true, cache);
	}
	async getLinkedModules(): Promise<string[]> {
		return (await this.runCmd<Paged<LinkedModulePath>>("lm -n=0", true, true))
			.entries;
	}

	async getModDeps(file: string): Promise<string[]> {
		return (
			await this.runCmd<Paged<string>>(
				`moddeps_for --direct --path=${file} --filter=[source_root=1] -n=0`,
				true,
				true,
			)
		).entries;
	}

	async getSourceRoot(): Promise<string> {
		return (
			await this.runCmd<{ source_root: string }>("source_root", true, true)
		).source_root;
	}

	async getRootPid(): Promise<number> {
		return (await this.runCmd<{ root_pid: number }>("root_pid", true, true))
			.root_pid;
	}

	async getExecutables(pid: string): Promise<any> {
		return (await this.runCmd<Paged<any>>(`execs --pid=${pid}`, true, true))
			.entries;
	}

	async getRevCompsFor(fname: string): Promise<any> {
		return (await this.runCmd<Paged<any>>(`revcomps_for --path=${fname}`, true))
			.entries;
	}

	async getProcess(
		pid: number | string,
		idx: number | string,
	): Promise<CASProcessResponse> {
		pid = typeof pid === "number" ? pid : parseInt(pid);
		idx = typeof idx === "number" ? idx : parseInt(idx);

		return JSON.parse(await this.runQuery(`proc_lookup?pid=${pid}&idx=${idx}`));
	}

	async getChildren(
		pid: number | string,
		idx: number | string,
		page?: number | boolean,
		recurse: false | number = false,
	): Promise<CASChildrenResult<Process>> {
		pid = typeof pid === "number" ? pid : parseInt(pid);
		idx = typeof idx === "number" ? idx : parseInt(idx);
		if (recurse !== false) {
			const res = await this.runQuery(
				`children?pid=${pid}&idx=${idx}&max_results=0&depth=${recurse}`,
				true,
			);
			return parseStream(res);
		}
		if (typeof page === "boolean" && page) {
			const res = await this.runQuery(
				`children?pid=${pid}&idx=${idx}&max_results=0`,
				true,
			);
			return parseStream(res);
			// const res = await this.getChildren(pid, idx, 0);
			// if (res.ERROR !== undefined) {
			// 	return res;
			// }
			// for (let i = 1; i <= res.page_max; i++) {
			// 	const nextPage = await this.getChildren(pid, idx, i);
			// 	if (nextPage.ERROR !== undefined) {
			// 		return res;
			// 	}
			// 	res.children.push(...nextPage.children);
			// }
			// return res;
		} else if (typeof page === "boolean") {
			page = 0;
		}

		return parseStream(
			await this.runQuery(`children?pid=${pid}&idx=${idx}&page=${page}`, true),
		);
	}

	async getProcessInfo(
		pid: number | string,
		idx: number | string,
		page?: number,
	): Promise<CASProcessInfoResponse> {
		pid = typeof pid === "number" ? pid : parseInt(pid);
		idx = typeof idx === "number" ? idx : parseInt(idx);
		page ??= 0;
		return JSON.parse(
			await this.runQuery(`proc?pid=${pid}&idx=${idx}&page=${page}`),
		);
	}
	//#endregion
}
