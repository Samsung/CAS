import { join } from "node:path/posix";
import { deserialize, serialize } from "node:v8";
import { decodeText, testAccess } from "@cas/helpers";
import { encode } from "@cas/helpers/base91.js";
import { http } from "@cas/http";
import { ArchiveSource, P4MappingType } from "@cas/manifest";
import { Err, EventualResult, Ok, type Result } from "@opliko/eventual-result";
import { EventEmitter, type ExtensionContext, Uri, workspace } from "vscode";
import type { Logger } from "winston";
import { loadDeps } from "../deps";
import { getLogger } from "../logger";
import settings from "../settings";
import { readJSONWithSchema } from "../utils";
import { ArchiveMap } from "./archives";
import { PathTrie, type PathTrieChildren } from "./path-trie";
import { commonPathPrefix, subPath, toRelative } from "./paths";
export interface MappingOptions {
	context: ExtensionContext;
	path: Uri | string;
	root: Uri | string;
	encoding?: string;
}

export interface ExpandedP4Mapping extends Omit<P4MappingType, "mapping"> {
	mapping: Record<string, string>;
}

/**
 * Map paths to the values corresponding to longest prefixes they match
 */
export class Mapping implements Disposable {
	//#region properties
	#mappingTrie: PathTrie<string>;
	#paths: string[];
	#root: string;
	#logger: Logger;
	#mappedPaths: Uri[];
	#archiveMap?: ArchiveMap;
	#context: ExtensionContext;
	#cleanPaths?: PathTrie<string>;
	#mappedTrie?: PathTrie<string>;
	#mappedTrieDone: boolean = false;
	#originalRoot?: string;
	mappings: ExpandedP4Mapping[];

	// getters
	get root() {
		return this.#root;
	}
	get paths() {
		return this.#paths;
	}
	get mapped() {
		return this.#mappedPaths;
	}
	get archiveMap() {
		return this.#archiveMap;
	}
	get mappedTrie() {
		return this.#mappedTrie;
	}
	// setters
	set root(root: string) {
		this.#root = root;
	}

	set originalRoot(root: string) {
		this.#originalRoot = root;
	}
	//#endregion

	//#region events
	newMapping = new EventEmitter<Uri>();
	newMappings = new EventEmitter<Uri[]>();
	//#endregion

	//#region initialization
	private constructor(
		context: ExtensionContext,
		mappings: ExpandedP4Mapping[],
		root?: string,
	) {
		this.mappings = mappings;
		this.#mappingTrie = PathTrie.from(
			mappings.reduce((mappings, mapping) => {
				return { ...mappings, ...mapping.mapping };
			}, {}),
		);
		this.#paths = [];
		this.#mappedPaths = [];
		this.#root = root ?? "TODO: add a default in ../perforce.ts";
		this.#context = context;

		this.#logger = getLogger("Mapping");
	}

	static async loadTemplateFromSettings({
		context,
		root,
	}: Omit<MappingOptions, "path">) {
		const mappings = settings.buildMappings.map((mapping) =>
			Mapping.parse(mapping),
		);
		return new Ok(
			new Mapping(context, mappings, root instanceof Uri ? root.fsPath : root),
		);
	}
	//#endregion
	//#region paths
	/**
	 * Parse P4-style mappings into an object
	 * @param p4mapping a list of P4 View style mappings (//remote/path/... //local/path/...)
	 * @returns object containing mappins formatted into a JS object
	 */
	static parse(p4mapping: P4MappingType): ExpandedP4Mapping {
		const result: Record<string, string> = {};
		const logger = getLogger("Mapping/parse");
		for (const mapping of p4mapping.mapping) {
			// eslint-disable-next-line prefer-const
			let [remote, local] = mapping.split(" ", 2);
			if (!remote || !local) {
				logger.warn(`Skipped invalid mapping: ${mapping}`);
				continue;
			}
			// ignore the template prefix, we'll be using relative paths from deps anyway
			local = local.slice(local.indexOf("/", local.indexOf("TEMPLATE")));
			if (p4mapping.rootPath) {
				local = join(p4mapping.rootPath, local);
			}
			// remove the wildcard
			result[local.replace("...", "")] = remote.replace("...", "");
		}

		return {
			...p4mapping,
			mapping: result,
		};
	}

	/**
	 * Add paths to create mappings for
	 * @param paths new paths to create mappings for
	 * @returns this instance of Mapping for chaining
	 */
	public addPaths(paths: string[]): Mapping {
		this.#mappedTrieDone = false;
		this.#paths = this.#paths.concat(paths);

		this.#logger.debug(
			`added ${paths.length} new paths, for a total of ${this.#paths.length}`,
		);
		return this;
	}

	static readonly #sanitizeMap = {
		"@": "%40",
		"#": "%23",
		"*": "%2A",
		"%": "%25",
		" ": "%20",
	} as const;
	public static sanitizePath(path: string) {
		return path.replace(
			/([@#\*% ])/g,
			(c) => Mapping.#sanitizeMap[c as "@" | "#" | "*" | "%"],
		);
	}

	/**
	 * Create P4 View-style mappings for all stored paths
	 * @returns list of mappings
	 */
	public async createMappings(
		depsFile: Uri,
		archiveSource?: ArchiveSource,
	): Promise<Result<string[], string[]>> {
		const knownHash = this.#context.workspaceState.get<string>(
			`depsHash:${depsFile.fsPath}`,
		);
		const newHash = encode(
			new Uint8Array(
				await crypto.subtle.digest(
					"SHA-256",
					serialize(await workspace.fs.stat(depsFile)),
				),
			),
		);

		const pathTrieCacheUri = Uri.joinPath(
			this.#context.storageUri!,
			`pathTrieCache.${knownHash}.bin`,
		);

		const pathsCacheUri = Uri.joinPath(
			this.#context.storageUri!,
			`pathsCache.${knownHash}.bin`,
		);

		let pathPrefix: string;
		if (
			newHash === knownHash &&
			(await testAccess(pathTrieCacheUri.fsPath)) &&
			(await testAccess(pathsCacheUri.fsPath))
		) {
			this.#mappedTrie = PathTrie.deserialize(
				await workspace.fs.readFile(pathTrieCacheUri),
			);
			this.#mappedTrieDone = true;
			this.#paths = deserialize(await workspace.fs.readFile(pathsCacheUri));
			this.originalRoot = this.#context.workspaceState.get<string>(
				`depsHash:${depsFile.fsPath}:originalRoot`,
				this.originalRoot,
			);

			pathPrefix = this.#context.workspaceState.get<string>(
				`depsHash:${depsFile.fsPath}:pathPrefix`,
				"",
			);
		} else {
			await new EventualResult(loadDeps(depsFile)).map(async (deps) => {
				this.originalRoot = deps.root;
				await this.#context.workspaceState.update(
					`depsHash:${depsFile.fsPath}:originalRoot`,
					deps.root,
				);
				return this.addPaths(deps.files);
			});
			pathPrefix = commonPathPrefix(this.paths);
			this.#mappedTrie = new PathTrie(pathPrefix);
			await this.#context.workspaceState.update(
				`depsHash:${depsFile.fsPath}:pathPrefix`,
				pathPrefix,
			);
		}

		this.#logger.debug(`Creating new mappings for ${this.#paths.length} paths`);
		const mappings: string[] = [];
		const failures: string[] = [];
		this.#cleanPaths = this.#cleanPaths?.getParent(pathPrefix!, true);

		const workspaceRoot = workspace.workspaceFolders?.at(0);
		this.#mappedTrie = new PathTrie(pathPrefix!);
		for (const path of this.paths) {
			const { prefix, value: remote } = this.#mappingTrie
				.get(path)
				.unwrapOr({ prefix: undefined, value: undefined });
			if (
				!prefix ||
				!remote ||
				(this.#cleanPaths && !this.#cleanPaths.has(path, true))
			) {
				this.#logger.warn(
					`${path} was not found under any path in mapping template or clean list`,
				);
				failures.push(path);
				continue;
			}
			const filePath = Mapping.sanitizePath(
				subPath(toRelative(prefix), toRelative(path)),
			);
			const mappingUri = Uri.file(
				join(workspaceRoot?.uri.fsPath ?? "./", filePath),
			);
			if (!this.#mappedTrieDone) {
				this.#mappedPaths.push(mappingUri);
				this.#mappedTrie.set(path, join(remote, filePath));
			}
		}
		// this.newMappings.fire(this.#mappedPaths);
		// TODO: check if it's possible to summarize quickly enough for it to actually be worth the effort
		// this.summarize();

		for (const [local, remote] of this.#mappedTrie.mappings()) {
			mappings.push(`/${remote} /${join(this.root, local)}`);
		}

		if (newHash !== knownHash) {
			await this.cleanCache();
			await workspace.fs.createDirectory(this.#context.storageUri!);
			await workspace.fs.writeFile(
				pathTrieCacheUri,
				new Uint8Array(this.#mappedTrie.serialize()),
			);
			await workspace.fs.writeFile(
				pathsCacheUri,
				new Uint8Array(serialize(this.#paths)),
			);

			await this.#context.workspaceState.update(
				`depsHash:${depsFile.fsPath}`,
				newHash,
			);
		}

		if (failures.length) {
			this.#logger.info(
				`${failures.length} paths not found in mapping template`,
			);
			if (!archiveSource) {
				return new Err(failures);
			}
			this.#archiveMap = new ArchiveMap(
				archiveSource,
				this.#context,
				failures,
				this.#originalRoot,
			);
		}
		return new Ok(mappings);
	}

	private async cleanCache() {
		for (const [file] of await workspace.fs.readDirectory(
			this.#context.storageUri!,
		)) {
			if (file.match(/^(pathTrieCache|mappedPathsCache)\..*\.bin$/)) {
				await workspace.fs.delete(
					Uri.joinPath(this.#context.storageUri!, file),
				);
			}
		}
	}

	private summarize() {
		if (!this.#mappedTrie || !this.#cleanPaths || this.#mappedTrieDone) {
			return;
		}
		for (const path of this.#paths) {
			const { prefix, children, value } = this.#mappedTrie
				.get(path.slice(0, path.lastIndexOf("/")))
				.mapOr(
					{ prefix: undefined, value: undefined, children: undefined },
					(v) => v,
				);
			if (!prefix || !children || children.has("...")) {
				continue;
			}
			const { children: cleanChildren } = this.#cleanPaths
				.get(prefix, true)
				.mapOr(
					{ prefix: undefined, value: undefined, children: undefined },
					(v) => v,
				);
			if (!cleanChildren) {
				continue;
			}
			let mappedPath = value?.slice(0, value.lastIndexOf("/")) ?? "";
			if (
				// don't summarize paths that have mappings to different repos
				[...children.values()].every((child) => {
					if (!child.value) {
						return true;
					}
					if (!mappedPath) {
						mappedPath = child.value.slice(0, child.value.lastIndexOf("/"));
						return true;
					}
					return child.value.startsWith(mappedPath);
				}) &&
				// only summarize if at most 25% of files are missing
				this.compareChildCount(children, cleanChildren) < 0.25
			) {
				children.clear();
				children.set(
					join(prefix, "..."),
					new PathTrie(join(prefix, "..."), join(mappedPath, "...")),
				);
			}
		}
		this.#mappedTrieDone = true;
	}

	private compareChildCount(
		dirty: PathTrieChildren<unknown> | undefined,
		clean: PathTrieChildren<unknown>,
	): number {
		if (dirty?.has("...")) {
			return 0;
		}
		let diff = 0;
		for (const [path, trie] of clean.entries()) {
			const dirtyTrie = dirty?.get(path);
			if (!dirtyTrie && !trie.children.size) {
				diff += 1 / clean.size;
			} else if (trie.children.size > 0) {
				diff +=
					this.compareChildCount(dirtyTrie?.children, trie.children) /
					clean.size;
			}
		}
		if (dirty) {
			for (const [path, _] of dirty.entries()) {
				if (!clean.get(path)) {
					diff -= 1 / dirty?.size;
				}
			}
		}
		return diff;
	}

	public isMapped(file: Uri | string): boolean {
		const fileUri = file instanceof Uri ? file : Uri.file(file);
		return this.#mappedPaths.some((path) => path.fsPath === fileUri.fsPath);
	}
	//#endregion

	[Symbol.dispose](): void {
		this.newMappings.dispose();
	}

	public async loadCleanList(
		path: string | Uri | URL,
	): Promise<Result<string, string>> {
		if (!(path instanceof Uri || path instanceof URL)) {
			path = path.startsWith("http") ? new URL(path) : Uri.file(path);
		}
		return (
			path instanceof Uri
				? new EventualResult(workspace.fs.readFile(path)).map(decodeText)
				: new EventualResult(http.get(path)).andThen<string, string>(
						async (r) =>
							r.ok ? new Ok(await r.text()) : new Err(r.statusText),
					)
		)
			.mapErr((e) => (e instanceof Error ? e.message : `${e}`))
			.effect((cleanlist) => {
				this.#cleanPaths = PathTrie.fromJSON(cleanlist);
			});
	}
}
