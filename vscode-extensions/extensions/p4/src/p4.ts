import { type ChildProcess, execFile } from "node:child_process";
import { availableParallelism } from "node:os";
import { join } from "node:path";
import { env } from "node:process";
import { Readable } from "node:stream";
import { testAccess } from "@cas/helpers/fs.js";
import { sleep } from "@cas/helpers/promise.js";
import { decodeText } from "@cas/helpers/text.js";
import {
	allResults,
	anyResult,
	Err,
	EventualResult,
	Ok,
	type Result,
} from "@opliko/eventual-result";
import { LRUCache } from "lru-cache";
import {
	EventEmitter,
	type FileSystemWatcher,
	FileType,
	type Progress,
	Uri,
	workspace,
} from "vscode";
import type { Logger } from "winston";
import { parse, stringify } from "yaml";
import { getLogger } from "./logger";
import { Mapping } from "./mapping/mapping";
import type { PathTrie } from "./mapping/path-trie";
import { commonPathPrefix } from "./mapping/paths";
import settings from "./settings";
import { confirm, type MaybeAsync, showInput } from "./utils";

function promisifyChild(child: ChildProcess) {
	return new Promise((resolve, reject) => {
		child.addListener("exit", resolve);
		child.addListener("error", reject);
	});
}

interface CommandOptions {
	env?: NodeJS.ProcessEnv;
	stdin?: string;
	signal?: AbortSignal;
}

export enum ClientStatus {
	Exists = 0,
	DifferentRoot = 1,
	DoesNotExist = 2,
}

/**
 * Perforce CLI communication handling
 */
export class P4 {
	//#region properties
	#logger: Logger;
	constructor() {
		this.#logger = getLogger("p4");
	}
	//#endregion

	//#region events
	synchronized = new EventEmitter();
	//#endregion

	//#region helpers
	/**
	 * Wrapper for running a given P4 CLI command as a simple async function
	 * Supports custom environments and passing a string to stdin
	 * @param command p4 command and arguments to run
	 * @param options addtional properties relevant to how the command is being ran
	 * @returns Result with either the command output or error message
	 */
	public async runCommand(
		command: string | string[],
		options?: CommandOptions,
	): Promise<Result<string, string>> {
		if (!Array.isArray(command)) {
			// split on spaces unless surrounded by quotes
			command = command
				.split(/(".*?"|[^"\s]+)+(?=\s*|\s*$)/g)
				.filter((s) => s.trim().length > 0);
		}
		// eslint-disable-next-line prefer-const
		let { stdin, env: p4env } = options ?? {};
		p4env = { ...env, ...p4env };
		if (settings.p4server) {
			p4env.P4PORT = settings.p4server;
		}

		const child = execFile(settings.p4Path, command, {
			encoding: "utf-8",
			env: p4env,
			signal: options?.signal,
		});
		if (stdin && child.stdin && child.stdin?.writable) {
			// TS sometimes has issues with promisify, like here where it thinks the result doesn't have any arguments
			const result = await new Promise((resolve) =>
				Readable.from(stdin, { encoding: "utf-8" })
					.pipe(child.stdin!)
					.on("finish", resolve),
			);
			if (result) {
				this.#logger.debug(`stdin write unsuccessful, result: ${result}`);
			}
			child.stdin?.end();
		}
		let out = "";
		let err = "";
		child.stdout?.on("data", (data) => (out += data));
		child.stderr?.on("data", (data) => (err += data));
		const result = await promisifyChild(child);
		if (err.length && P4.isActuallyError(err)) {
			this.#logger.error(err);
			return new Err(`error: ${result}`);
		}
		if (err.length) {
			out += err;
		}
		return new Ok(out);
	}

	/**
	 * For *some* reasons p4 CLI sometimes writes totally innocuous status information to stderr
	 * This function tries to reject the most obvious non-errors from causing issues with error handling
	 * @param err message written out to stderr
	 */
	private static isActuallyError(err: string) {
		if (err.match(/.+ - file\(s\) up-to-date./)) {
			return false;
		}
		return true;
	}
	//#endregion

	//#region commands
	/**
	 * Reload an unloaded workspace, with a couple of retries
	 * @param name
	 * @returns result containing either reloaded workspace or error message
	 */
	@ensureLoggedIn
	private async reload(name: string): Promise<Result<string, string>> {
		const command = `reload -c ${name}`;
		for (let i = 0; i < settings.maxP4Tries; i++) {
			const out = (await this.runCommand(command))
				.effectErr((err) => {
					this.#logger.warn(`failed reloading workspace: ${err}`);
				})
				.unwrapOr("unloaded");
			if (out.includes("reloaded")) {
				return new Ok(out);
			}
			// simple exponential backoff
			await sleep(4 ** i);
		}
		this.#logger.error(
			`failed to reload workspace ${settings.maxP4Tries} times, giving up`,
		);
		return new Err("unloaded");
	}

	/**
	 * Create a new P4 workspace in a given root folder
	 * @param name for the new workspace
	 * @param root folder the workspace will map files to
	 * @param mappings list of P4 View-style mappings
	 * @returns output of `p4 client` command creating the final workspace
	 */
	@ensureLoggedIn
	public async createWorkspace(
		name: string,
		root: Uri | string,
		mappings: string[],
		mappedFiles: PathTrie<string>,
		changelist?: number,
		partialChangelists?: number[],
		progress?: Progress<{ message?: string; increment?: number }>,
		signal?: AbortSignal,
	) {
		if (root instanceof Uri) {
			root = root.fsPath;
		}
		const workspaceFile = Uri.joinPath(Uri.file(root), "workspace.p4.yml");

		const loadingController = new AbortController();
		const workspaceData = await new EventualResult(
			Promise.any([
				new EventualResult(testAccess(workspaceFile.fsPath))
					.andThen((access) =>
						access
							? new Ok(access)
							: new Err(`Workspace file not found: ${workspaceFile}`),
					)
					.map(() => workspace.fs.readFile(workspaceFile))
					.map(decodeText)
					.unwrap(),
				this.runCommand(`client -o ${name}`, {
					env: { P4CLIENT: name },
					signal: AbortSignal.any([
						...(signal ? [signal] : []),
						loadingController.signal,
					]),
				}),
			]),
		)
			// ensure workspace is loaded
			.andThen(async (data: string) => {
				loadingController.abort();
				if (data?.includes("unloaded") ?? true) {
					return this.reload(name);
				}
				return new Ok(data);
			})
			// change view to block style to avoid yaml parser folding newlines
			.map((document) => {
				return document.replace("View:\n", "View: >\n").replaceAll("\t", "  ");
			})
			// parse as yaml - with the above style change this should be compatible
			.map(parse)
			.mapErr((err) => this.#logger.error(`failed to create workspace: ${err}`))
			.unwrapOr(undefined);

		if (!workspaceData) {
			return new Err("failed to create workspace");
		}

		workspaceData.View = mappings.join("\n");
		workspaceData.Root = Mapping.sanitizePath(root.toString());

		const options = workspaceData.Options.split(" ") as string[];
		workspaceData.Options = options
			.filter((opt: string) => opt !== "noallwrite")
			.concat("allwrite")
			.join(" ");

		workspaceData.Description =
			"Dependency-based workspace created by cas-p4 extension";

		const newWorkspace = stringify(workspaceData, {
			defaultKeyType: "PLAIN",
			lineWidth: 0,
		}).replaceAll(/(?<=\w+):\s*(>-?|\|-?)\n/gi, ":\n");
		const writePromise = workspace.fs.writeFile(
			workspaceFile,
			new TextEncoder().encode(newWorkspace),
		);
		progress?.report({
			increment: 5,
			message: "Creating P4 workspace",
		});
		let watcher: FileSystemWatcher;
		const incrementAfter = mappings.length / 50;
		const deletions: Uri[] = [];
		const filePrefix = commonPathPrefix(
			mappings.map(
				(m) =>
					m
						.split(" ", 2)
						.at(-1)
						?.slice(3 + name.length) ?? "",
			),
		);
		return new EventualResult(
			this.runCommand("client -i", {
				env: { P4CLIENT: name },
				stdin: newWorkspace,
				signal,
			}),
		)
			.effect(() => {
				progress?.report({
					message: "Synchronizing workspace",
					increment: 10,
				});
			})
			.effect(async () => {
				await workspace.fs.createDirectory(
					Uri.joinPath(Uri.file(root), filePrefix),
				);
				watcher = workspace.createFileSystemWatcher(
					join(filePrefix, "**", "*"),
					false,
					true,
					true,
				);
				let counter = 0;
				watcher.onDidCreate((file) => {
					const path = file.path.slice(file.path.indexOf(filePrefix));
					if (!mappedFiles.has(path, true)) {
						return deletions.push(file);
					}
					counter += 1;
					if (counter >= incrementAfter) {
						progress?.report({ increment: 1 });
						counter = 0;
					}
					return;
				});
			})
			.andThen(
				async () =>
					await this.sync(name, true, changelist, partialChangelists, signal),
			)
			.effect(() => {
				progress?.report({ increment: 1, message: "Deleting client" });
			})
			.andThen(async () => await this.delete(name, signal))
			.effect(async () => {
				await Promise.allSettled([
					writePromise,
					...deletions.map((f) =>
						workspace.fs.delete(f, {
							useTrash: false,
						}),
					),
					...(await this.deleteUnused(
						Uri.joinPath(Uri.file(root), filePrefix),
						mappedFiles,
						filePrefix,
					)),
				]);
			})
			.finally(() => {
				watcher.dispose();
			});
	}

	private async deleteUnused(
		folder: Uri,
		trie: PathTrie<string>,
		prefix: string,
	) {
		const deletionPromises: Promise<unknown>[] = [];
		let files: [string, FileType][];
		try {
			files = await workspace.fs.readDirectory(folder);
		} catch (_) {
			return deletionPromises;
		}
		if (!files.length) {
			return [
				workspace.fs.delete(folder, {
					recursive: true,
					useTrash: false,
				}) as Promise<unknown>,
			];
		}
		for (const [name, type] of files) {
			if (
				!trie.has(
					Uri.joinPath(folder, name).path.slice(folder.path.indexOf(prefix)),
					true,
				)
			) {
				deletionPromises.push(
					(
						workspace.fs.delete(Uri.joinPath(folder, name), {
							recursive: true,
							useTrash: false,
						}) as Promise<unknown>
					).catch(() => undefined),
				);
			}
			if (type === FileType.Directory) {
				deletionPromises.push(
					this.deleteUnused(Uri.joinPath(folder, name), trie, prefix).then(
						(deletions) => Promise.allSettled(deletions),
					),
				);
			}
		}
		return deletionPromises;
	}

	/**
	 * Synchronize all files in a workspace
	 * @param name of the workspace to sync
	 * @param changelist to synchronize to, by default just syncs to latest
	 * @param parallel enables parallel synchronization, defaults to true
	 * @returns output of p4 sync command
	 */
	private async sync(
		name: string,
		parallel = true,
		changelist?: number,
		partialChangelists?: number[],
		signal?: AbortSignal,
	): Promise<Result<string, string>> {
		const parallelArg = `--parallel=threads=${availableParallelism()}`;
		this.#logger.debug(
			`running sync ${changelist ? `on changelist ${changelist}` : "on latest files"}${partialChangelists ? ` and partials ${partialChangelists.join(", ")}` : ""}`,
		);
		const maxPartial = Math.max(...(partialChangelists ?? [0]));

		return new EventualResult(
			this.runCommand(
				[
					"-q",
					"sync",
					parallel ? parallelArg : undefined,
					changelist ? `@${changelist}` : undefined,
				].filter(Boolean) as string[],
				{
					env: { P4CLIENT: name },
					signal,
				},
			),
		).andThen<string, string>((r) => {
			if (partialChangelists?.length) {
				return new EventualResult(
					this.runCommand(
						[
							"-q",
							"sync",
							parallel ? parallelArg : undefined,
							changelist ? `@${changelist},${maxPartial}` : undefined,
						].filter(Boolean) as string[],
						{
							env: { P4CLIENT: name },
							signal,
						},
					),
					// ignore partial sync errors
				)
					.effectErr((e) => {
						this.#logger.warn(`partial sync failed: ${e}`);
					})
					.or(new Ok(r).eventually());
			}
			return new Ok(r);
		});
	}

	/**
	 * delete a workspace
	 */
	@ensureLoggedIn
	public async delete(name: string, signal?: AbortSignal) {
		return this.runCommand(`client -d ${name}`, {
			env: { P4CLIENT: name },
			signal,
		});
	}

	public async exists({
		name,
		root,
	}: {
		name?: string;
		root?: string | Uri;
	}): Promise<ClientStatus> {
		if (root instanceof Uri) {
			root = root.fsPath;
		}
		const files = await workspace.findFiles("workspace.p4.{yml,yaml}");
		for (const file of files) {
			try {
				const contents = await workspace.fs.readFile(file);
				const client = parse(decodeText(contents).replaceAll("\t", "  "));
				if (client && client.Client === name) {
					if (client.Root !== root) {
						return ClientStatus.DifferentRoot;
					}
					return ClientStatus.Exists;
				}
			} catch (_e) {
				this.#logger.debug(`Failed to read ${file}`);
			}
		}
		return ClientStatus.DoesNotExist;
	}
	public async loginStatus(): Promise<Result<boolean, unknown>> {
		return new EventualResult(this.runCommand("login -s"))
			.map((result) => result.includes("ticket expires in"))
			.orElse((_) => new Ok(false).eventually());
	}
	//#endregion

	async checkAndLogin() {
		this.#logger.debug("checking login state");
		return new EventualResult(this.loginStatus())
			.andThen(async (v) => (v ? new Ok(false) : this.login()))
			.effect((v) => {
				this.#logger.debug(v ? "user logged in" : "user was already logged in");
			})
			.effectErr((e) => {
				this.#logger.error(`error: ${e}`);
			});
	}
	//#region UI

	public async login(): Promise<Result<boolean, string>> {
		return new EventualResult(
			confirm(
				"You need to sign in to Perforce, do you want to continue?",
				true,
			),
		)
			.andThen((v) => v.okOr("user cancelled"))
			.effectErr((e) => {
				this.#logger.error(e);
			})
			.andThen((_) =>
				showInput({
					title: "P4 Password",
					prompt: "Password",
					password: true,
				}),
			)
			.andThen((v) => v.okOr("no password given"))
			.effectErr((e) => {
				this.#logger.error(e);
			})
			.andThen((password) =>
				this.runCommand("login", {
					stdin: password,
					env: {
						P4PASSWD: password,
					},
				}),
			)
			.effect((_) => {
				this.#logger.info("signed in");
			})
			.andThen<boolean, string>((result) =>
				result.includes("logged in.")
					? new Ok(true)
					: new Err(`login failed: ${result}`),
			);
	}

	//#endregion
}
const lru = new LRUCache({ ttl: 1000 * 60 * 60 * 5, ttlAutopurge: true });

function ensureLoggedIn<
	This extends InstanceType<typeof P4>,
	Args extends any[],
>(
	method: (this: This, ...args: Args) => MaybeAsync<Result<any, any>>,
	_context: ClassMethodDecoratorContext<
		This,
		(this: This, ...args: Args) => MaybeAsync<Result<any, any>>
	>,
) {
	async function replacementMethod(this: This, ...args: Args) {
		if (lru.get(process.env.P4USER ?? "p4")) {
			return method.call(this, ...args);
		}
		return new EventualResult(this.checkAndLogin())
			.effect(() => {
				lru.set(process.env.P4USER ?? "p4", true);
			})
			.andThen(() => method.call(this, ...args));
	}
	return replacementMethod;
}
