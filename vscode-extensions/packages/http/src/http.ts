import { mkdir, mkdtemp } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { ReadableStream } from "node:stream/web";
import ky, {
	type AfterResponseHook,
	type BeforeRequestHook,
	type BeforeRetryHook,
	HTTPError,
	type KyInstance,
} from "ky";
import { type ExtensionContext, Uri, window } from "vscode";
import { DiskCache } from "./cache";

export let http: KyInstance;

function createAuthHook(context: ExtensionContext): BeforeRequestHook {
	return async (request) => {
		if (!request.headers.has("Authorization")) {
			const url = new URL(request.url);
			const [username, password] = await Promise.all([
				context.secrets.get(`cas:http:username:${url.host}`),
				context.secrets.get(`cas:http:password:${url.host}`),
			]);
			if (username || password) {
				request.headers.set(
					"Authorization",
					`Basic ${btoa(`${username ?? ""}:${password ?? ""}`)}`,
				);
			}
		}
	};
}
function createAuthRetryHook(context: ExtensionContext): BeforeRetryHook {
	return async ({ request, error }) => {
		if (error instanceof HTTPError) {
			const url = new URL(request.url);
			const response = error.response;
			if (
				response.status === 401 &&
				response.headers.get("www-authenticate")?.startsWith("Basic")
			) {
				const username = await window.showInputBox({
					title: "Username",
					prompt: `Username for ${url.host}`,
					ignoreFocusOut: true,
				});
				const password = await window.showInputBox({
					title: "Password",
					prompt: `Password for ${username} on ${url.host}`,
					ignoreFocusOut: true,
					password: true,
				});
				if (!username || !password) {
					return;
				}
				await Promise.all([
					context.secrets.store(`cas:http:username:${url.host}`, username),
					context.secrets.store(`cas:http:password:${url.host}`, password),
				]);
				request.headers.set(
					"Authorization",
					`Basic ${btoa(`${username}:${password}`)}`,
				);
			}
		}
	};
}

function createGetCacheHook(cache: DiskCache): BeforeRequestHook {
	return async (request: Request) => {
		// handle disabling cache
		if (request.cache === "no-cache") {
			return;
		}
		if (["GET", "HEAD"].includes(request.method)) {
			const cacheData = await (request.method === "GET"
				? cache.getWithMetadata(request.url)
				: cache.getMetadata(request.url)
			).catch(() => undefined);
			if (cacheData) {
				return new Response(
					"data" in cacheData
						? new Uint8Array(cacheData.data).buffer
						: undefined,
					{
						headers: {
							"Content-Type": (cacheData.metadata as Record<string, string>)
								.contentType,
							Digest: cacheData.integrity,
							cache: "no-cache",
						},
						status: 200,
					},
				);
			}
		}
		return;
	};
}

function createSetCacheHook(cache: DiskCache): AfterResponseHook {
	return async (request, _options, response) => {
		// handle disabling cache
		if (
			["no-store", "no-cache"].includes(request.cache) ||
			["no-store", "no-cache"].includes(response.headers.get("cache") ?? "")
		) {
			return;
		}

		const digest =
			(response.headers
				.get("Digest")
				?.replace(/sha-(\d+)=/, (_, alg) => `sha${alg}-`) ??
			response.headers.has("X-Checksum-Sha256"))
				? `sha256-${Buffer.from(response.headers.get("X-Checksum-Sha256") ?? "", "hex").toString("base64")}`
				: undefined;
		if (request.method === "GET" && response.ok && response.body) {
			const [cacheStream, responseStream] =
				response.body?.tee() ?? new ReadableStream().tee();
			setImmediate(() =>
				cache.setStream(
					request.url,
					cacheStream,
					{
						contentType: response.headers.get("Content-Type"),
					},
					digest,
				),
			);
			return new Response(responseStream, response);
		}
		if (request.method === "HEAD") {
			await cache.setMetadata(
				request.url,
				{
					contentType: response.headers.get("Content-Type"),
				},
				digest,
			);
		}
		return;
	};
}

export async function setupApi(context: ExtensionContext) {
	const cachePath = context.storageUri
		? Uri.joinPath(context.storageUri, "fetch-cache").fsPath
		: await mkdtemp(join(tmpdir(), "cas"));
	await mkdir(cachePath, { recursive: true });
	// LRU cache with 7 day retention period
	const cache = new DiskCache(cachePath, 1000 * 60 * 60 * 24 * 7);
	http = ky.create({
		hooks: {
			beforeRequest: [createGetCacheHook(cache), createAuthHook(context)],
			beforeRetry: [createAuthRetryHook(context)],
			afterResponse: [createSetCacheHook(cache)],
		},
		retry: {
			statusCodes: [401, 408, 413, 429, 500, 502, 503, 504],
		},
	});
}
