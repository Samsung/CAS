import type { ProcessInfo, ProcessWithChildren } from "@cas/types/bas.js";
import { Temporal } from "temporal-polyfill";
import type { eid } from "./types";
// polyfills for upcoming time-related ECMAScript features (proposal-temporal and proposal-intl-duration-format)
// TODO: remove once all supported Node.JS versions support these features natively (supported from 23+)
import "@formatjs/intl-durationformat/polyfill";

export function stopPropagation<F extends Function>(
	func?: F,
): (event: UIEvent) => F {
	return (event: UIEvent) => {
		event.stopPropagation();
		if (func) {
			return func();
		}
		return;
	};
}

export function aborted(signal: AbortSignal): Promise<Event> {
	return new Promise((resolve) => signal.addEventListener("abort", resolve));
}

export function* chunk<T>(arr: T[], len: number, initial: number = 0) {
	if (initial) {
		yield arr.slice(0, initial);
	}
	for (let i = initial; i < arr.length; i += len) {
		yield arr.slice(i, i + len);
	}
}

// @ts-ignore this is polyfilled, so TS doesn't understand it exists here
const durationFormat = new Intl.DurationFormat("en", { style: "narrow" });
export function nsFormat(
	time: number,
	smallestUnit: Temporal.SmallestUnit<Temporal.DateTimeUnit> = "millisecond",
): string | undefined {
	// TODO: add locale setting
	const formatted = durationFormat.format(
		Temporal.Duration.from({ nanoseconds: time }).round({
			largestUnit: "day",
			smallestUnit: smallestUnit,
			roundingIncrement: 1,
		}),
	);
	return formatted.length ? formatted : undefined;
}

export const emptyInfo: ProcessInfo = {
	class: "command",
	pid: 0,
	idx: 0,
	pidx: 0,
	ppid: 0,
	wpid: 0,
	bin: "",
	children: 0,
	cmd: [],
	cwd: "",
	pipe_eids: [],
	open_len: 0,
	etime: 0,
	files_pages: 0,
	openfiles: [],
};
export function eidToStr(eid: number, idx: number): string;
export function eidToStr(eid: eid): string;
export function eidToStr(eid: eid | number, idx?: number) {
	return `${typeof eid !== "number" ? eid.pid : eid}:${typeof eid !== "number" ? eid.idx : idx}`;
}

export function eidEq(eid1?: eid, eid2?: eid) {
	return eid1 && eid2 && eid1.idx === eid2.idx && eid1.pid === eid2.pid;
}

export function decodeHtmlCharCodes(str: string): string {
	return str.replace(/(&#(\d+);)/g, (_match, _capture, charCode) =>
		String.fromCharCode(charCode),
	);
}

export function procHasChildren(proc: ProcessWithChildren) {
	return Array.isArray(proc.children)
		? !!proc.children.length
		: !!proc.children;
}
