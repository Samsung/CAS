export interface OGSearchResult {
	line: string;
	lineNumber: string;
	tag: string | null;
}

export interface OGSearchResults {
	time: number;
	resultCount: number;
	startDocument: number;
	endDocument: number;
	results: {
		[path: string]: OGSearchResult[];
	};
}
