import { History, type HistoryOptions } from "@cas/helpers/history.js";
import { dequal } from "dequal";

/**
 * A simple history stack with undo and redo functionality. It will only keep the last `limit` items.
 * If you want to keep all items, set `limit` to 0
 */
export class SvelteHistory<T> implements Iterable<T>, History<T> {
	internalArray: T[] = $state([]);
	present: number;
	limit: number;
	constructor(
		initial: T[] | T | undefined,
		options: HistoryOptions = { limit: 100 },
	) {
		const { limit } = options;
		this.internalArray = Array.isArray(initial)
			? initial?.slice(Math.max(initial?.length - 100, 0), -1)
			: initial
				? [initial]
				: [];
		this.present = this.internalArray.length - 1;
		this.limit = limit;
	}

	get length() {
		return this.internalArray.length;
	}

	/**
	 *
	 * @returns current item or undefined if there is no current item (history is empty)
	 */
	current(): T | undefined {
		return this.internalArray.at(this.present);
	}

	/**
	 * Add a new item to history
	 * @param item - item to push into history
	 */
	push(item: T) {
		// ensure the array contains just past elements + 1 spot for the new item
		this.present += 1;
		this.internalArray.length = this.present + 1;
		// replace last item or empty spot with new item
		this.internalArray[this.internalArray.length - 1] = item;
		if (this.limit && this.length >= this.limit) {
			this.present = Math.max(0, this.present - 1);
			this.internalArray.shift();
		}
		return this.internalArray.length;
	}

	/**
	 * Go back in history
	 * @returns the current item in the history stack
	 */
	undo() {
		this.present = Math.max(this.present - 1, 0);
		return this.current();
	}
	/**
	 * Go back to the future :)
	 * @returns the current item in the history stack
	 */
	redo() {
		this.present = Math.min(this.present + 1, this.internalArray.length - 1);
		return this.current();
	}

	/**
	 * Fully clear the history
	 */
	clear() {
		this.internalArray.length = 0;
		this.present = -1;
	}

	/**
	 * Check if element exists in history
	 * @param item
	 * @returns
	 */
	has(item: T): boolean {
		return this.internalArray.some((i) => dequal(i, item));
	}

	/**
	 * Return counts of each element
	 */
	counts(): Map<T, number> {
		return this.internalArray.reduce<Map<T, number>>(
			(counts: Map<T, number>, element: T) =>
				counts.set(element, (counts.get(element) ?? 0) + 1),
			new Map<T, number>(),
		);
	}

	/**
	 * Get a deep copy of the internal array
	 */
	get array() {
		return structuredClone(this.internalArray);
	}

	[Symbol.iterator]() {
		return this.internalArray[Symbol.iterator]();
	}

	find(
		predicate: (value: T, index: number, obj: T[]) => unknown,
		thisArg?: any,
	): T | undefined {
		return this.internalArray.find(predicate, thisArg);
	}
}
