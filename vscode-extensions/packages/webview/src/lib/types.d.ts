import type { Node } from "./treeview/types";

import type { Process, ProcessInfo } from "@cas/types/bas.ts";

export type eid = { pid: number; idx: number };

export interface ProcNode extends Node {
	eid: eid;
	hasParent: boolean | undefined;
	parent?: ProcNode | null;
	data: Process;
}

export type EventData =
	| ProcessEventData
	| InitEventData
	| SetChildrenEventData
	| ProcessInfoEventData
	| SetSrvUrlEventData
	| ProcessesEventData
	| SetAncestorsEvent
	| SearchResults;

export interface InitEventData {
	func: "init";
	eid: eid;
	data: {
		pid_list: eid[];
		server_url: string;
	};
}
export interface ProcessEventData {
	func: "setProcess";
	eid: eid;
	data: Process;
}
export interface ProcessesEventData {
	func: "setProcesses";
	eid: eid;
	data: {
		count: number;
		execs: Process[];
	};
}

export interface SetChildrenEventData {
	func: "setChildren";
	eid: eid;
	data: {
		children: Process[];
		count: number;
		pages: number;
		page: number;
	};
}

export interface ProcessInfoEventData {
	func: "setProcInfo";
	eid: eid;
	data: ProcessInfo;
}

export interface SetSrvUrlEventData {
	func: "setSrvUrl";
	eid: eid;
	data: {
		// TODO
	};
}

export interface SetDeps {
	//TODO
}

export interface SetAncestorsEvent {
	func: "setAncestors";
	eid: eid;
	data: {
		ancestors: Process[];
	};
}

export interface SearchResults {
	func: "searchResults";
	eid: eid;
	data: {
		ERROR?: string;
		count: number;
		execs?: Process[];
	};
}

export type InputEventFor<T> = InputEvent & { currentTarget: EventTarget & T };

export type MaybeAsync<T> = Promise<T> | T;
