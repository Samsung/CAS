import { Process, ProcessInfo } from "./bas";

export interface Paged<T> {
	count: number;
	page: number;
	page_max: number;
	entries_per_page: number;
	num_entries: number;
	entries: T[];
}

export interface CASSuccessResults<T> extends Paged<T> {
	ERROR?: undefined;
}

export interface CASError {
	ERROR: string;
	count: 0;
}

export type CASResult<T = unknown> = CASSuccessResults<T> | CASError;

export interface ModelStatus {
	db_dir: string;
	nfsdb_path: string | null;
	deps_path: string | null;
	ftdb_paths: string[] | null;
	config_path: string;
	loaded_nfsdb: string | null;
	loaded_ftdb: string | null;
	loaded_config: string | null;
	last_access: string | null;
	image_version: number;
	db_verison: string;
	host: string;
}
export interface ServerStatus {
	[server: `http${string}`]: {
		[model: string]: ModelStatus;
	};
}

export interface CasJSONError {
	ERROR: string;
}
export type CASProcessResponse = Process | CasJSONError;
export type CASProcessInfoResponse = ProcessInfo | CasJSONError;

export type CASChildrenResult<T> = Exclude<CASResult<T>, "entries"> & {
	children: T[];
};
