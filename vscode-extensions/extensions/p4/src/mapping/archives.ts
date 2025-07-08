import { getMaxListeners, setMaxListeners } from "node:events";
import { createWriteStream } from "node:fs";
import { mkdir } from "node:fs/promises";
import { dirname, extname, join } from "node:path";
import { Readable } from "node:stream";
import { pipeline } from "node:stream/promises";
import { ReadableStream } from "node:stream/web";
import { getHeapStatistics } from "node:v8";
import { batchProcess, testAccess } from "@cas/helpers";
import { http } from "@cas/http";
import { ArchiveSource } from "@cas/manifest";
import Bzip2 from "@foxglove/wasm-bz2";
import { Err, EventualResult, Ok, type Result } from "@opliko/eventual-result";
import { fs, type ZipDirectoryEntry, type ZipFileEntry } from "@zip.js/zip.js";
import {
	type ExtensionContext,
	type Progress,
	ProgressLocation,
	Uri,
	window,
	workspace,
} from "vscode";
import type { Logger } from "winston";
import { getLogger } from "../logger";
import settings from "../settings";

export class ArchiveMap {
	#artifactory: boolean = false;
	#archive: URL;
	#paths: Set<string>;
	#logger: Logger;
	#originalRoot: string;
	#context: ExtensionContext;
	constructor(
		archivePath: ArchiveSource,
		context: ExtensionContext,
		paths?: string[],
		originalRoot?: string,
	) {
		if (typeof archivePath === "object") {
			this.#artifactory = archivePath.artifactory;
			archivePath = archivePath.url;
		}
		this.#archive = URL.canParse(archivePath)
			? new URL(archivePath)
			: new URL(Uri.file(archivePath).toString());
		this.#paths = new Set(paths);
		this.#context = context;
		this.#logger = getLogger("ArchiveMap");
		this.#originalRoot = originalRoot ?? "/";
	}

	addPaths(paths: string[] | string[][]) {
		for (const path of paths.flat()) {
			this.#paths.add(path);
		}
	}
	public async remaining() {
		const fsPromises: Promise<void>[] = [];
		for (const path of this.#paths) {
			fsPromises.push(
				testAccess(path).then((exists) => {
					exists && this.#paths.delete(path);
				}),
			);
		}
		await Promise.all(fsPromises);
		return this.#paths;
	}

	private async checkArchiveURL(
		signal?: AbortSignal,
	): Promise<"tar" | "zip" | null> {
		const response = await http.head(this.#archive, { signal, timeout: 5000 });
		if (response.status < 200 || response.status >= 300) {
			this.#logger.error(
				`request for archive file failed with status ${response.status}`,
			);
			return null;
		}
		const type = response.headers.get("Content-Type");
		if (
			type &&
			[
				"application/zip",
				"application/x-tar",
				"application/octet-stream",
			].includes(type)
		) {
			// assume that generic octet streams are tar files; technically not correct, but they'll more commonly have no extension
			return type === "application/zip" ? "zip" : "tar";
		}
		return null;
	}

	private async downloadFromArchive(
		prefix: string,
		signal?: AbortSignal,
		progress?: Progress<{
			message?: string;
			increment?: number;
		}>,
	): Promise<Result<number, string>> {
		const initialSize = this.#paths.size;
		return new EventualResult(async () => {
			if (signal) {
				setMaxListeners(
					Math.max(getMaxListeners(signal), this.#paths.size + 1),
				);
			}
			const progressSize = 100 / this.#paths.size;
			const zipFs = new fs.FS();

			let headers = new Headers();
			const [username, password] = await Promise.all([
				this.#context.secrets.get(`cas:http:username:${this.#archive.host}`),
				this.#context.secrets.get(`cas:http:password:${this.#archive.host}`),
			]);
			if (username || password) {
				headers.set(
					"Authorization",
					`Basic ${btoa(`${username ?? ""}:${password ?? ""}`)}`,
				);
			}

			await zipFs.importHttpContent(this.#archive.toString(), {
				useRangeHeader: true,
				headers,
				signal,
			});

			const errors = [];

			for await (const paths of batchProcess(
				this.#paths,
				async (path) => {
					const entry = zipFs.find(path) as
						| ZipFileEntry<unknown, unknown>
						| ZipDirectoryEntry;
					if (entry && !entry.directory) {
						try {
							await workspace.fs.writeFile(
								Uri.file(join(prefix, path)),
								await decompressed(entry),
							);
						} catch (e) {
							errors.push(path);
							throw e;
						}
						return path;
					}
				},
				100,
				true,
			)) {
				for (const path of paths) {
					if (path) {
						this.#paths.delete(path);
						progress?.report({ increment: progressSize });
					}
				}
			}
			return errors.length;
		}).andThen<number, string>(async (errorCount) => {
			this.#logger.debug("done");
			// this.#logger.debug(archivePaths);
			if (errorCount === 0) {
				await this.#context.workspaceState.update("archiveExtracted", true);
			}
			if (this.#paths.size > 0) {
				return new Err(
					`not all missing paths found in archive! ${this.#paths.size} (of original ${initialSize}) still missing`,
				);
			}
			return new Ok(initialSize);
		});
	}

	private async downloadFromArtifactory(
		prefix: string,
		signal?: AbortSignal,
		progress?: Progress<{
			message?: string;
			increment?: number;
		}>,
	): Promise<Result<number, string>> {
		const initialSize = this.#paths.size;
		return new EventualResult(async () => {
			if (signal) {
				setMaxListeners(
					Math.max(getMaxListeners(signal), this.#paths.size + 1),
				);
			}
			const progressSize = 100 / this.#paths.size;
			return Promise.allSettled(
				[...this.#paths.values()].map(async (path) => {
					const start = Date.now();
					const result = await http.get(
						new URL(
							`${this.#archive.pathname}!${join(this.#originalRoot, path)}`,
							this.#archive,
						),
						{ signal, timeout: 600 * 1000 },
					);
					this.#logger.info(`took ${(Date.now() - start) / 1000} seconds`);
					if (!result.ok || !result.body) {
						this.#logger.warn(`path not found: ${path}`);
						return;
					}

					this.#paths.delete(path);
					await mkdir(dirname(join(prefix, path)), {
						recursive: true,
					});
					await pipeline(
						Readable.fromWeb(result.body),
						createWriteStream(join(prefix, path), {
							encoding: "utf-8",
						}),
					);
					progress?.report({ increment: progressSize });
				}),
			);
		})
			.effect((promises) => {
				for (const promise of promises) {
					if (promise.status === "rejected") {
						this.#logger.error(promise.reason);
					}
				}
			})
			.andThen<number, string>(() => {
				this.#logger.debug("done");
				// this.#logger.debug(archivePaths);
				if (this.#paths.size > 0) {
					return new Err(
						`not all missing paths found in archive! ${this.#paths.size} (of original ${initialSize}) still missing`,
					);
				}
				return new Ok(initialSize);
			});
	}

	async extractPaths(
		prefix: string,
		signal?: AbortSignal,
		progress?: Progress<{
			message?: string;
			increment?: number;
		}>,
	): Promise<Result<number, string>> {
		progress?.report({ message: "Testing artifact archive URL" });
		await this.checkArchiveURL(signal);
		progress?.report({ message: "Extracting artifacts", increment: 1 });
		return window.withProgress(
			{
				title: "Downloading artifacts",
				location: ProgressLocation.Notification,
			},
			async (progress) => {
				if (
					this.#artifactory &&
					settings.get("useArtifactoryExtraction", false)
				) {
					return this.downloadFromArtifactory(prefix, signal, progress);
				}
				return this.downloadFromArchive(prefix, signal, progress);
			},
		);
	}
}

let bz2: Bzip2 | undefined = undefined;
async function decompressed(
	entry: ZipFileEntry<unknown, unknown>,
): Promise<Uint8Array> {
	switch (entry.data?.compressionMethod) {
		case 0x0c: {
			if (!bz2) {
				bz2 = await Bzip2.init();
			}
			const { heap_size_limit, total_heap_size } = getHeapStatistics();
			return bz2.decompress(
				await entry.getUint8Array({ passThrough: true }),
				entry.uncompressedSize,
				{
					// use the small mode less than 1GiB of heap space is left
					small: heap_size_limit - total_heap_size < 1024 * 1024 * 1024,
				},
			);
		}
		// let the lib handle this by default
		default:
			return entry.getUint8Array();
	}
}
