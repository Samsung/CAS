import { Readable } from "node:stream";
import { finished } from "node:stream/promises";
import type { ReadableStream } from "node:stream/web";
import {
	type CacheObject,
	type GetCacheObject,
	get,
	index,
	ls,
	put,
	rm,
	verify,
} from "cacache";
import { Logger } from "winston";

type MaybeAsync<T> = Promise<T> | T;

interface LRUMetadata {
	timestamp: number;
}

export class DiskCache implements AsyncMap<string, Buffer> {
	#storagePath: string;
	#ttl: number;
	#cleanupTimeout?: NodeJS.Timeout;
	#logger: Logger;
	constructor(
		storagePath: string,
		ttl?: number,
		getLogger?: (name: string) => Logger,
	) {
		this.#storagePath = storagePath;
		this.#ttl = ttl ?? -1;
		if (getLogger) {
			this.#logger = getLogger(`cache (${storagePath})`);
		} else {
			this.#logger = new Logger();
		}
	}
	async clear(): Promise<void> {
		return rm.all(this.#storagePath);
	}
	async delete(key: string): Promise<boolean> {
		const info = await get.info(this.#storagePath, key).catch(() => undefined);
		if (info) {
			await rm(this.#storagePath, key);
		}
		return !!info;
	}
	async forEach(
		callbackfn: (
			value: Buffer,
			key: string,
			map: this,
			object: CacheObject & { size: number },
		) => MaybeAsync<void>,
	): Promise<void> {
		await Promise.all(
			Object.entries(await ls(this.#storagePath)).map(async ([key, value]) =>
				callbackfn((await get(this.#storagePath, key)).data, key, this, value),
			),
		);
	}
	async get(key: string): Promise<Buffer | undefined> {
		const result = await get(this.#storagePath, key).catch(() => undefined);
		if (
			result?.metadata &&
			this.#ttl >= 0 &&
			(result.metadata as LRUMetadata).timestamp + this.#ttl < Date.now()
		) {
			await this.delete(key);
			return;
		}
		if (result && (result.metadata as LRUMetadata).timestamp) {
			await index.insert(this.#storagePath, key, result?.integrity, {
				metadata: {
					...(result.metadata as LRUMetadata),
					timestamp: Date.now(),
				},
				size: result?.size,
			});
		}
		this.scheduleCleanup();
		return result?.data;
	}
	async getWithMetadata(key: string): Promise<GetCacheObject | undefined> {
		const result = await get(this.#storagePath, key).catch(() => undefined);
		if (
			result?.metadata &&
			this.#ttl >= 0 &&
			(result.metadata as LRUMetadata).timestamp + this.#ttl < Date.now()
		) {
			await this.delete(key);
			return;
		}
		if (result && (result?.metadata as LRUMetadata).timestamp) {
			await index.insert(this.#storagePath, key, result.integrity, {
				metadata: {
					...(result.metadata as LRUMetadata),
					timestamp: Date.now(),
				},
				size: result?.size,
			});
		}
		this.scheduleCleanup();
		return result;
	}
	async getMetadata(key: string): Promise<CacheObject | undefined> {
		const result = await get
			.info(this.#storagePath, key)
			.catch(() => undefined);
		if (
			result?.metadata &&
			this.#ttl >= 0 &&
			(result.metadata as LRUMetadata).timestamp + this.#ttl < Date.now()
		) {
			await this.delete(key);
			return;
		}
		if (result && (result?.metadata as LRUMetadata).timestamp) {
			await index.insert(this.#storagePath, key, result.integrity, {
				metadata: {
					...(result.metadata as LRUMetadata),
					timestamp: Date.now(),
				},
				size: result?.size,
			});
		}
		this.scheduleCleanup();
		return result;
	}
	async has(key: string): Promise<boolean> {
		const info = get.info(this.#storagePath, key).catch(() => undefined);
		return info !== undefined;
	}
	async set<Metadata extends Record<string, unknown>>(
		key: string,
		value: Buffer,
		metadata?: Metadata,
	): Promise<this> {
		await put(this.#storagePath, key, value, {
			metadata,
		});
		return this;
	}

	async setMetadata<Metadata extends Record<string, unknown>>(
		key: string,
		metadata?: Metadata,
		digest?: string | null,
	): Promise<this> {
		await index.insert(this.#storagePath, key, digest ?? "", {
			metadata,
			size: 0,
		});
		return this;
	}

	async clean() {
		await verify(this.#storagePath);
	}

	async setStream<Metadata extends Record<string, unknown>>(
		key: string,
		value: ReadableStream,
		metadata?: Metadata,
		digest?: string,
	): Promise<this> {
		const cacheStream = put.stream(this.#storagePath, key, {
			metadata,
			integrity: digest,
		});
		await finished(Readable.fromWeb(value).pipe(cacheStream));
		return this;
	}

	get size(): Promise<number> {
		return ls(this.#storagePath).then(
			(data: ls.Cache) => Object.keys(data).length,
		);
	}
	async *entries(): AsyncIterableIterator<[string, Buffer]> {
		const lstream = ls.stream(this.#storagePath) as AsyncIterable<CacheObject>;
		for await (const { key } of lstream) {
			yield [key, (await get(this.#storagePath, key)).data];
		}
	}
	async *keys(): AsyncIterableIterator<string> {
		const lstream = ls.stream(this.#storagePath) as AsyncIterable<CacheObject>;
		for await (const { key } of lstream) {
			yield key;
		}
	}
	async *values(): AsyncIterableIterator<Buffer> {
		const lstream = ls.stream(this.#storagePath) as AsyncIterable<CacheObject>;
		for await (const { key } of lstream) {
			yield (await get(this.#storagePath, key)).data;
		}
	}
	async *[Symbol.iterator](): AsyncIterableIterator<[string, Buffer]> {
		yield* this.entries();
	}
	get [Symbol.toStringTag](): string {
		return "DiskCache";
	}

	private scheduleCleanup() {
		if (this.#cleanupTimeout) {
			clearTimeout(this.#cleanupTimeout);
		}
		this.#cleanupTimeout = setTimeout(async () => {
			const ttl = this.#ttl;
			await verify(this.#storagePath, {
				filter(entry: CacheObject) {
					return (
						!(entry?.metadata as LRUMetadata).timestamp ||
						(entry?.metadata as LRUMetadata).timestamp + ttl > Date.now()
					);
				},
			});
			// try not to run while the plugin is doing something
		}, 5000);
	}
}

interface AsyncMap<K, V> {
	clear(): Promise<void>;
	/**
	 * @returns true if an element in the Map existed and has been removed, or false if the element does not exist.
	 */
	delete(key: K): Promise<boolean>;
	/**
	 * Executes a provided function once per each key/value pair in the Map, in insertion order.
	 */
	forEach(callbackfn: (value: V, key: K, map: this) => void): Promise<void>;
	/**
	 * Returns a specified element from the Map object. If the value that is associated to the provided key is an object, then you will get a reference to that object and any change made to that object will effectively modify it inside the Map.
	 * @returns Returns the element associated with the specified key. If no element is associated with the specified key, undefined is returned.
	 */
	get(key: K): Promise<V | undefined>;
	/**
	 * @returns boolean indicating whether an element with the specified key exists or not.
	 */
	has(key: K): Promise<boolean>;
	/**
	 * Adds a new element with a specified key and value to the Map. If an element with the same key already exists, the element will be updated.
	 */
	set(key: K, value: V): Promise<this>;
	/**
	 * @returns the number of elements in the Map.
	 */
	readonly size: Promise<number>;
}
