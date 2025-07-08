export interface CasApiEvent<T = unknown> {
	func: string;
	id: string;
	data: T;
}

export interface VSCodeContext<T = string> {
	type: string;
	v: T;
}
