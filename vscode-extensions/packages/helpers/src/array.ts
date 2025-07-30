/**
 * @module ArrayHelpers
 */

export type TypedArray =
	| Int8Array
	| Uint8Array
	| Uint8ClampedArray
	| Int16Array
	| Uint16Array
	| Int32Array
	| Uint32Array
	| Float32Array
	| Float64Array;

/**
 * A generator returning chunks of given max size from the array
 * @param arr array to chunk
 * @param size max size of each chunk
 */

export function* chunk<T>(arr: Iterable<T>, size: number): Generator<T[]> {
	let acc: T[] = Array(size);
	let i = 0;
	for (const item of arr) {
		if (i === size) {
			i = 0;
			yield acc;
			acc = Array(size);
		}
		acc[i++] = item;
	}
	yield acc.slice(0, i);
}
