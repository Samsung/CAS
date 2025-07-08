import * as v from "valibot";

export const depSchema = v.object({
	filename: v.string(),
	original_path: v.string(),
	open_timestamp: v.pipe(v.string(), v.digits()),
	close_timestamp: v.pipe(v.string(), v.digits()),
	ppid: v.number(),
	type: v.pipe(v.string(), v.picklist(["plain", "compiled", "linked"])),
	access: v.pipe(v.string(), v.picklist(["r", "w", "rw"])),
	exists: v.pipe(v.number(), v.picklist([0, 1])),
	link: v.pipe(v.number(), v.picklist([0, 1])),
	workspace_path: v.optional(v.string()),
	binary: v.optional(v.boolean()),
});

const depSchemav2 = v.required(depSchema, ["workspace_path", "binary"]);

const depsMetadata = {
	count: v.number(),
	page: v.number(),
	page_max: v.number(),
	entries_per_page: v.number(),
	num_entries: v.number(),
};

export const depsSchema = v.variant("version", [
	v.object({
		version: v.undefinedable(v.literal(1), 1),
		entries: v.array(depSchema),
		...depsMetadata,
	}),
	v.object({
		version: v.literal(2),
		entries: v.array(depSchemav2),
		...depsMetadata,
	}),
]);

export type DepSchema = v.InferOutput<typeof depSchema>;

export type DepsSchema = v.InferOutput<typeof depsSchema>;
