export type Eid = { pid: string | number; idx: string | number };
export type EidStr = `${number}:${number}`;
export type Peid = { ppid: string | number; pidx: string | number };

export type LinkedModulePath = string;

export type Bin = string;

export interface Dep {
	path: string;
	num_deps: number;
	parent: Eid;
}

export interface CommandInfo {
	class: ProcessClass;
	pid: number;
	idx: number;
	pidx: number;
	ppid: number;
	cwd: string;
	bin: string;
	command: string;
}

export type ProcessClass = "compiler" | "linker" | "command";
export interface Process extends Omit<CommandInfo, "command"> {
	cmd: string[];
	pipe_eids: number[];
	stime?: number;
	etime: number;
	children: number;
	wpid: number;
	open_len: number;
}

export type FileMode = "r" | "w" | "rw";

interface GenericProcessInfo extends Process {
	class: ProcessClass;
	files_pages: number;
	openfiles: [FileMode, string][];
}

export interface CommandProcessInfo extends GenericProcessInfo {
	class: "command";
}

type SourceType = "c" | "c++" | "other";
export interface CompilerProcessInfo extends GenericProcessInfo {
	class: "compiler";
	src_type: SourceType;
	compiled: {
		[path: string]: SourceType;
	}[];
	objects: string[];
	headers: string[];
	ipaths: string[];
	defs: string[];
	undefs: string[];
}
export interface LinkerProcessInfo extends GenericProcessInfo {
	class: "linker";
	linked: string;
}
export type ProcessInfo =
	| CommandProcessInfo
	| CompilerProcessInfo
	| LinkerProcessInfo;

export type FileType = "plain" | "compiled" | "linked";

export interface BASFile {
	filename: string;
	original_path: string;
	workspace_path: string;
	open_timestamp: `${number}`;
	close_timestamp: `${number}`;
	ppid: number;
	type: FileType;
	access: FileMode;
	exists: 0 | 1;
	link: 0 | 1;
}

export type CompCommands = CompCommandEntry[];

export type CompCommandEntry =
	| {
			arguments: string[];
			directory: string;
			file: string;
	  }
	| {
			command: string;
			directory: string;
			file: string;
	  };

export interface ProcessWithChildren extends Omit<Process, "children"> {
	children: number | ProcessWithChildren[];
}
