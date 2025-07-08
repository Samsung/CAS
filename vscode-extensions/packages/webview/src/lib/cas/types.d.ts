import type { BASFile } from "@cas/types/bas.js";
import type { Map } from "svelte/reactivity";

export interface CasCommand {
	pid: number;
	idx: number;
	ppid: number;
	pidx: number;
	bin: string;
	cwd: string;
	class: string;
	linked: string;
	compiled: string;
	command: string;
}

export interface CasAccess {
	pid: number;
	access: string;
}

interface CasDef {
	name: string;
	value: string;
}

export interface CasDefs {
	command: CasCommand;
	compiled_files: string[];
	include_paths: string[];
	headers: string[];
	source_type: string;
	defs: CasDef[];
}

export type entry = string | BASFile | CasCommand | CasAccess | CasDefs;

export type SingleTypeMap<T, U> = U extends any ? Map<string, U> : never;
export type SingleTypeArray<T> = T extends any ? T[] : never;
type EntryArray = SingleTypeArray<entry>;
export interface CASSuccessResults {
	count: number;
	page: number;
	page_max: number;
	entries_per_page: number;
	num_entries: number;
	entries: EntryArray;
	ERROR?: undefined;
}

export interface CASError {
	ERROR: string;
	count: 0;
}

export type CASResults = CASSuccessResults | CASError;

type page = number;
type entries = number;
type command = string;
export type historyEntry = [command, page, entries];
