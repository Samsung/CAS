import * as v from "valibot";

export const OGSearchQuerySchema = v.object({
	full: v.optional(v.string()),
	def: v.optional(v.string()),
	symbol: v.optional(v.string()),
	path: v.optional(v.string()),
	hist: v.optional(v.string()),
	type: v.optional(v.string()),
	projects: v.optional(v.array(v.string())),
	maxresults: v.optional(v.pipe(v.number(), v.minValue(1))),
	start: v.optional(v.pipe(v.number(), v.minValue(0))),
});

export type OGSearchQuery = v.InferOutput<typeof OGSearchQuerySchema>;

export const OGSearchResult = v.object({
	line: v.string(),
	lineNumber: v.pipe(
		v.string(),
		v.transform((v) => parseInt(v.length ? v : "1")),
		v.integer(),
		v.minValue(1),
	),
	tag: v.nullable(v.string()),
});
export type OGSearchResult = v.InferOutput<typeof OGSearchResult>;

export const OGSearchResults = v.object({
	time: v.pipe(v.number(), v.integer(), v.minValue(0)),
	resultCount: v.pipe(v.number(), v.integer(), v.minValue(-1)),
	startDocument: v.pipe(v.number(), v.integer(), v.minValue(-1)),
	endDocument: v.pipe(v.number(), v.integer(), v.minValue(-1)),
	results: v.record(v.string(), v.array(OGSearchResult)),
});
export type OGSearchResults = v.InferOutput<typeof OGSearchResults>;

export const OGDefinition = v.object({
	type: v.string(),
	signature: v.nullable(v.string()),
	text: v.string(),
	symbol: v.string(),
	lineStart: v.pipe(v.number(), v.integer()),
	lineEnd: v.pipe(v.number(), v.integer()),
	line: v.pipe(v.number(), v.integer()),
	namespace: v.nullable(v.string()),
});
export const OGDefinitions = v.array(OGDefinition);
export type OGDefinitions = v.InferOutput<typeof OGDefinitions>;
