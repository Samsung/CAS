/**
 * @module PromiseHelpers
 */

import { chunk } from "./array";

export async function withAbort<T>(
	promise: MaybePromise<T>,
	abortSignal?: AbortSignal,
): Promise<T> {
	return abortSignal
		? Promise.race<T>([
				promise,
				new Promise((_, reject) => {
					abortSignal.addEventListener("abort", () =>
						reject(new Error("Aborted")),
					);
				}),
			])
		: promise;
}
export async function sleep(ms: number, signal?: AbortSignal) {
	return new Promise((resolve) => {
		const timeoutId = setTimeout(resolve, ms);
		signal?.addEventListener("abort", () => {
			clearTimeout(timeoutId);
			resolve(undefined);
		});
	});
}

export function promiseWithResolvers<T>(
	signal?: AbortSignal,
): PromiseWithResolvers<T> {
	const result = Promise.withResolvers<T>();
	signal?.addEventListener("abort", () => result.reject("aborted"));
	return result;
}

export async function* exponentialBackoff({
	initial,
	base = 2,
	max,
	condition,
	signal,
}: {
	initial: number;
	base: number;
	max?: number;
	condition?: (i: number) => boolean;
	signal?: AbortSignal;
}) {
	let i = 0;
	while ((condition?.(i) ?? true) && !signal?.aborted) {
		yield i;
		await sleep(Math.min(initial * base ** i, max ?? Infinity), signal);
	}
}

export type MaybePromise<T> = T | Promise<T>;

export async function* batchProcess<T, R>(
	items: Iterable<T>,
	process: (item: T) => Promise<R>,
	batchSize = 10,
	ignoreErrors = false,
): AsyncGenerator<R[]> {
	for (const itemChunk of chunk(items, batchSize)) {
		yield ignoreErrors
			? Promise.allSettled(itemChunk.map(process)).then((arr) =>
					arr.filter((v) => v.status === "fulfilled").map((v) => v.value),
				)
			: Promise.all(itemChunk.map(process));
	}
}

export async function* queueProcess<
	T,
	R,
	I extends Iterable<T> & { push: (...items: T[]) => number },
>(
	items: I,
	process: (item: T, items: I) => Promise<R>,
	batchSize = 10,
	ignoreErrors = false,
	itemsAreQueue = false,
	waitBetweenBatches = 0,
): AsyncGenerator<R[]> {
	const accPromises = Array(batchSize);
	let i = 0;
	for (const item of items) {
		accPromises[i++] = process(item, items);
		if (i >= batchSize) {
			i = 0;
			yield await (ignoreErrors
				? Promise.allSettled(accPromises).then((arr) =>
						arr.filter((v) => v.status === "fulfilled").map((v) => v.value),
					)
				: Promise.all(accPromises));
			if (waitBetweenBatches) {
				await sleep(waitBetweenBatches);
			}
		}
	}
	if (i > 0) {
		yield await (ignoreErrors
			? Promise.allSettled(accPromises).then((arr) =>
					arr.filter((v) => v.status === "fulfilled").map((v) => v.value),
				)
			: Promise.all(accPromises));
		if (itemsAreQueue) {
			// if the invocation extended the queue, continue
			yield* queueProcess(
				items,
				process,
				batchSize,
				ignoreErrors,
				itemsAreQueue,
				waitBetweenBatches,
			);
		}
	}
}
