import { join } from "node:path/posix";
import { deserialize, serialize } from "node:v8";
import { TypedArray } from "@cas/helpers";
import { Err, Ok, type Result } from "@opliko/eventual-result";
import { commonPathPrefix, subPath, suffix, toRelative } from "./paths";

export type PathTrieChildren<T> = Map<string, PathTrie<T>>;
export type TrieReturn<T> = {
	prefix?: string;
	value?: T;
	children?: PathTrieChildren<T>;
	this?: PathTrie<T>;
};
/**
 * A Trie data structure for efficiently finding the longest prefix a path corresponds to
 * and mapping it to a given value (here, P4 depot)
 */
export class PathTrie<T> {
	//#region properties
	#children: PathTrieChildren<T>;
	#value?: T;
	#key: string;
	public get key() {
		return this.#key;
	}
	public get value(): T | undefined {
		return this.#value;
	}
	public set value(value: T | undefined) {
		this.#value = value;
	}
	public get children(): PathTrieChildren<T> {
		return this.#children;
	}
	protected set children(children: PathTrieChildren<T>) {
		this.#children = children;
	}
	protected set key(key: string) {
		this.#key = key;
	}

	public get entries(): IterableIterator<[string, PathTrie<T>]> {
		return this.#children.entries();
	}
	public get size(): number {
		return this.#children.size;
	}
	//#endregion

	//#region initialization
	constructor(key: string, value?: T, children?: PathTrieChildren<T>) {
		this.#children = children ?? new Map();
		this.#key = key;
		this.#value = value;
	}
	/**
	 * Create a new trie from a given object
	 * @param kv an object with paths and values to add
	 */
	static from<T>(kv: Record<string, T>): PathTrie<T> {
		const root = commonPathPrefix(Object.keys(kv));
		const trie = new PathTrie<T>(toRelative(root), kv[root]);
		trie.addMany(kv);
		return trie;
	}

	static fromArray(arr: string[], root?: string): PathTrie<undefined> {
		root ??= commonPathPrefix(arr);
		const trie = new PathTrie<undefined>(toRelative(root), undefined);
		for (const key of arr) {
			trie.set(key);
		}
		return trie;
	}
	//#endregion

	//#region adding paths
	/**
	 * Add multiple entries at once
	 * @param kv an object with paths and values to add
	 */
	public addMany(kv: Record<string, T>) {
		for (const [key, value] of Object.entries(kv)) {
			this.set(key, value);
		}
	}

	/**
	 * Set a value for a given path, adding it to the trie in the process (if not already set)
	 * @param key path to set the value for
	 * @param value a new value
	 * @returns nothing
	 */
	public set(key: string, value?: T): PathTrie<T>;
	public set(key: string[], value?: T): PathTrie<T>;
	public set(key: string[] | string, value?: T): PathTrie<T> {
		if (!Array.isArray(key)) {
			if (!key.startsWith(this.key)) {
				return this;
			}
			key = toRelative(key)
				.slice(this.key.length)
				.split("/")
				.filter((s) => s.length);
		} else if (key[0] !== this.key) {
			return this;
		} else {
			key.shift();
		}
		if (!key.length) {
			return this;
		}

		// if subPath is empty the keys are equal - so we just overwrite the value with the newer one
		if (!key.length) {
			this.value = value;
			return this;
		}
		for (const [child, childNode] of this.#children.entries()) {
			if (key[0] === child) {
				return childNode.set(key, value);
			}
		}
		const childKey = key[0];
		const child = new PathTrie(childKey, value);
		if (key.length > 1) {
			child.set(key, value);
		}
		this.#children.set(childKey, child);
		return this;
	}
	//#endregion
	//#region trie search
	/**
	 * Search the trie for the longest matching prefix or exact key
	 * @param key string to find the value of the longest matching prefix for
	 * @returns found prefix and value - with their values being undefiend if not found
	 */
	public get(
		key: string,
		exact?: boolean,
		path?: string[],
	): Result<TrieReturn<T>, string>;
	get(
		key: string[],
		exact?: boolean,
		path?: string[],
	): Result<TrieReturn<T>, string>;
	public get(
		key: string | string[],
		exact = false,
		path: string[] = [],
	): Result<TrieReturn<T>, string> {
		if (!Array.isArray(key)) {
			if (!key.startsWith(this.key)) {
				return new Err(this.key);
			}
			key = toRelative(key)
				.slice(this.key.length)
				.split("/")
				.filter((s) => s.length);
		} else if (key[0] !== this.key) {
			return new Err(this.key);
		} else {
			key.shift();
		}

		if (path.at(0)?.endsWith("/")) {
			if (path[0].length === 1) {
				path.shift();
			} else {
				path[0] = path[0].slice(0, -1);
			}
		}

		// check all children for a matching prefix
		for (const [prefix, child] of this.children.entries()) {
			// if any child matches, dig deeper
			if (key[0] === prefix) {
				return child.get(key, exact, [...path, this.key]);
			}
		}
		if (exact && key.length) {
			return new Err(this.key);
		}
		// if no children match, we've found the longest prefix
		return new Ok({
			prefix: [...path, this.key].join("/"),
			value: this.value,
			children: this.children,
			this: this,
		});
	}

	/**
	 * find the first matching node which the given key is the child of
	 * @param key a key to find the parent of
	 * @param includePath whether to consider first part of the given key as the parent or the key before it
	 */
	public getParent(key: string, includePath?: boolean): PathTrie<T> | undefined;
	public getParent(
		key: string[],
		includePath?: boolean,
		top?: PathTrie<T>,
	): PathTrie<T> | undefined;
	public getParent(
		key: string | string[],
		includePath = false,
		top?: PathTrie<T>,
	): PathTrie<T> | undefined {
		if (!Array.isArray(key)) {
			key = toRelative(key)
				.split("/")
				.filter((s) => s.length);
		}
		if (includePath) {
			top = this;
			if (this.key === key[0]) {
				const result = this.getParent(key.slice(1), true, top);
				if (result) {
					return this;
				}
			}
		}
		const children = new Map(this.children);
		// BFS search for the parent node.
		for (const [childKey, child] of children) {
			// immediately return if we found final part
			if (key.length === 1 && key[0] === childKey) {
				return top ?? this;
			}
			// go deeper if we found any other part
			const partFound = key[0] === childKey;
			if (partFound) {
				const childResult = child.getParent(
					key.slice(+partFound),
					includePath,
					partFound ? (top ?? this) : undefined,
				);
				if (childResult) {
					return childResult;
				}
			}
			// clean up the map
			children.delete(childKey);
			// add all children to the loop
			for (const [k, v] of child.children) {
				children.set(k, v);
			}
		}
		return;
	}

	/**
	 * Check if path is in the trie
	 * @param key path to search for
	 * @param full if true, will only return true if the exact path is found in the trie. Otherwise will just check if there is a matching prefix
	 * @returns boolean indicating whether the path is in the trie
	 */
	public has(
		key: string | string[],
		exact = false,
		upToFolder = false,
	): boolean {
		if (!Array.isArray(key)) {
			if (!key.startsWith(this.key)) {
				return false;
			}
			key = toRelative(key)
				.slice(this.key.length)
				.split("/")
				.filter((s) => s.length);
		} else if (key[0] !== this.key) {
			return false;
		} else if (exact) {
			key.shift();
		} else {
			// if not exact we only need to check the first path
			return true;
		}
		// check all children for a matching prefix
		for (const [prefix, child] of this.children.entries()) {
			// if any child matches, dig deeper
			if (key[0] === prefix) {
				return child.has(key, exact);
			}
		}
		// length = 0 for full path match or 1 for folder match
		if (key.length !== +upToFolder) {
			return false;
		}
		// if no children match, we've found the longest prefix
		return true;
	}
	//#endregion

	//#region mappings

	public *mappings(
		prevPath?: string,
	): IterableIterator<[string, T | undefined]> {
		prevPath ??= this.key;
		for (const [path, child] of this.entries) {
			if (child.children.has("...")) {
				yield [
					join(prevPath, path, "..."),
					child.children.get("...")?.value ?? this.value,
				];
			} else if (!child.size) {
				yield [join(prevPath, path), child.value ?? this.value];
			} else {
				yield* child.mappings(join(prevPath, path));
			}
		}
	}

	//#endregion

	public toJSON(): string {
		return JSON.stringify({ [this.#key]: this.toObj() });
	}
	private toObj(): JSONObj<T> {
		return {
			value: this.#value,
			children: this.#children.size
				? Object.fromEntries(
						[...this.#children.entries()].map(([k, v]) => [k, v.toObj()]),
					)
				: undefined,
		};
	}

	private static unslash<T>(
		key: string,
		obj: JSONObj<T>,
	): [string, JSONObj<T>] {
		if (key.includes("/")) {
			const [first, ...rest] = key.split("/").filter((s) => s.length);
			return [first, { [rest.join("/")]: obj }];
		}
		return [key, obj];
	}
	private static fromObj<T>(key: string, obj: JSONObj<T>): PathTrie<T> {
		if (key.includes("/")) {
			return PathTrie.fromObj(...PathTrie.unslash(key, obj));
		}
		const { value, children } = obj;
		if (!children && !value) {
			return new PathTrie(
				key,
				undefined,
				new Map<string, PathTrie<T>>(
					Object.entries(obj)
						.map(([k, v]) => PathTrie.unslash<T>(k, v))
						.map(([k, v]) => [k, PathTrie.fromObj(k, v)]),
				),
			);
		}
		const trie = new PathTrie<T>(key, value);
		trie.children = children
			? new Map<string, PathTrie<T>>(
					Object.entries(children)
						.map(([k, v]) => PathTrie.unslash<T>(k, v))
						.map(([k, v]) => [k, PathTrie.fromObj(k, v)]),
				)
			: new Map();
		return trie;
	}
	public static fromJSON<T>(json: string) {
		const obj = JSON.parse(json);
		return PathTrie.fromObj<T>(
			...(Object.entries(obj)[0] as [string, JSONObj<T>]),
		);
	}

	public serialize() {
		return serialize({ [this.#key]: this.toObj() });
	}
	public static deserialize<T>(data: TypedArray) {
		return PathTrie.fromObj<T>(
			...(Object.entries(deserialize(data))[0] as [string, JSONObj<T>]),
		);
	}
}

interface JSONObj<T> {
	value?: T;
	children?: Record<string, JSONObj<T>>;
}
