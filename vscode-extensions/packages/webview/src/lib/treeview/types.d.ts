export interface Node {
	hasChildren: boolean;
	expanded: boolean;
	data: unknown;
	children: TreeType<T>;
}

export type TreeType<T extends Node> = T[];
