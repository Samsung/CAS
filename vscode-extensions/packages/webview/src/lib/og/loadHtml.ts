import { type Remote, wrap } from "comlink";
import { join } from "pathe";
import { browser } from "$app/environment";
import loadHtmlWorker from "$lib/og/loadHtmlWorker?worker&inline";

let workerLoaders: Remote<typeof import("$lib/og/loadHtmlHelpers")>[] = [];

let loadedWorkers = false;
let localLoadHtml:
	| typeof import("$lib/og/loadHtmlHelpers").loadHtml
	| undefined = undefined;
if (browser && import.meta.env.PROD) {
	const threads = globalThis.navigator?.hardwareConcurrency;
	try {
		for (let i = 0; i < threads; i++) {
			workerLoaders.push(
				wrap(
					new loadHtmlWorker({
						name: join(
							document.querySelector("base")?.href ?? "localhost",
							"_app/immutable/workers/",
						),
					}),
				),
			);
		}
		loadedWorkers = true;
	} catch (e) {
		console.log(e);
		workerLoaders = [];
	}
} else if (import.meta.env.DEV) {
	localLoadHtml = (await import("$lib/og/loadHtmlHelpers")).loadHtml;
}
function* loaderIter(): Iterator<
	typeof import("$lib/og/loadHtmlHelpers").loadHtml
> {
	let i = 0;
	while (true) {
		yield workerLoaders[i++].loadHtml;
		if (i >= workerLoaders.length) {
			i = 0;
		}
	}
}
let loader: ReturnType<typeof loaderIter>;
function getWorkerLoader(): typeof import("$lib/og/loadHtmlHelpers").loadHtml {
	if (!loader) {
		loader = loaderIter();
	}
	return loader.next().value;
}

export async function loadHtml(
	path: string,
	file: string,
	lines: number[],
	theme: string,
	fullQuery: string,
): Promise<string> {
	// worker only works in production because of https://github.com/vitejs/vite/pull/14366
	return loadedWorkers
		? await getWorkerLoader()(path, file, lines, theme, fullQuery)
		: ((await localLoadHtml?.(path, file, lines, theme, fullQuery)) ?? file);
}
