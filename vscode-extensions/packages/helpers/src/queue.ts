export class SimpleQueue<T> implements IterableIterator<T> {
	readonly #arr: T[] = [];
	constructor(initialItems: T[]) {
		this.#arr.push(...initialItems);
	}
	push(...items: T[]): number {
		return this.#arr.push(...items);
	}
	shift(): T | undefined {
		return this.#arr.shift();
	}
	get length(): number {
		return this.#arr.length;
	}
	get empty(): boolean {
		return !this.#arr.length;
	}
	[Symbol.iterator](): IterableIterator<T> {
		return this;
	}
	next(): IteratorResult<T, any> {
		// if (!this.#arr.length) {
		//     throw new Error("queue is empty");
		// }
		return {
			done: this.empty,
			value: this.shift()!,
		};
	}
}
