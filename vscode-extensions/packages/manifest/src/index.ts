import * as v from "valibot";

let resolve = (x: string) => x;
try {
	({ resolve } = require("@cas/vscode-variables"));
} catch {}

const stringWithVariables = v.pipe(v.string(), v.transform(resolve));

const url = v.pipe(stringWithVariables, v.url());
const urlOrIp = v.union([
	url,
	v.pipe(
		stringWithVariables,
		v.regex(
			/(?:(?:[1-9]|1\d|2[0-4])?\d|25[0-5])(?:\.(?:(?:[1-9]|1\d|2[0-4])?\d|25[0-5])){3}(:\d+)?/,
		),
	),
	v.pipe(
		stringWithVariables,
		v.regex(
			/\[?(?:(?:[\da-f]{1,4}:){7}[\da-f]{1,4}|(?:[\da-f]{1,4}:){1,7}:|(?:[\da-f]{1,4}:){1,6}:[\da-f]{1,4}|(?:[\da-f]{1,4}:){1,5}(?::[\da-f]{1,4}){1,2}|(?:[\da-f]{1,4}:){1,4}(?::[\da-f]{1,4}){1,3}|(?:[\da-f]{1,4}:){1,3}(?::[\da-f]{1,4}){1,4}|(?:[\da-f]{1,4}:){1,2}(?::[\da-f]{1,4}){1,5}|[\da-f]{1,4}:(?::[\da-f]{1,4}){1,6}|:(?:(?::[\da-f]{1,4}){1,7}|:)|fe80:(?::[\da-f]{0,4}){0,4}%[\da-z]+|::(?:f{4}(?::0{1,4})?:)?(?:(?:25[0-5]|(?:2[0-4]|1?\d)?\d)\.){3}(?:25[0-5]|(?:2[0-4]|1?\d)?\d)|(?:[\da-f]{1,4}:){1,4}:(?:(?:25[0-5]|(?:2[0-4]|1?\d)?\d)\.){3}(?:25[0-5]|(?:2[0-4]|1?\d)?\d))\]?(:\d+)?/,
		),
	),
]);
const path = v.union([
	v.pipe(
		stringWithVariables,
		v.regex(/^[a-zA-Z]:\\(((?![<>:"/\\|?*]).)+((?<![ .])\\)?)*$/),
	),
	v.pipe(stringWithVariables, v.regex(/((?:[^\/]*\/)*)(.*)/)),
]);
const urlOrFile = v.union([url, path]);

const optionalObj = <
	const TWrapped extends v.BaseSchema<unknown, unknown, v.BaseIssue<unknown>>,
	const TDefault extends v.Default<TWrapped, undefined>,
>(
	wrapped: TWrapped,
	default_: TDefault,
) =>
	v.optional(
		v.union([
			wrapped,
			v.pipe(
				v.strictObject({}),
				v.transform((_): TDefault => default_),
			),
		]),
		default_,
	);
const archiveSource = v.union([
	url,
	path,
	v.object({
		url: url,
		artifactory: v.optional(v.boolean(), false),
	}),
]);
export type ArchiveSource = v.InferOutput<typeof archiveSource>;

export const P4Mapping = v.object({
	clSync: v.pipe(v.number(), v.integer()),
	clPartial: v.array(v.pipe(v.number(), v.integer())),
	mapping: v.array(
		v.pipe(
			stringWithVariables,
			v.regex(
				/^\/\/([^@#*\n](?<!\.\.\.|%%\d))+(\.\.\.|\*|@[^@#*\n]+|#[^@#*\n]+)? \/\/([^@#\n](?<!%%\d))+(\.\.\.)?$/,
			),
		),
	),
	rootPath: path,
});
export type P4MappingType = v.InferOutput<typeof P4Mapping>;

export const ManifestSchema = v.object({
	version: optionalObj(
		v.union([
			v.pipe(
				v.string(),
				v.regex(
					/^(?<major>0|[1-9]\d*)(\.(?<minor>0|[1-9]\d*)(\.(?<patch>0|[1-9]\d*)(?:-(?<prerelease>(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*)(?:\.(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*))*))?(?:\+(?<buildmetadata>[0-9a-zA-Z-]+(?:\.[0-9a-zA-Z-]+)*))?)?)?$/,
				),
			),
			v.number(),
		]),
		"1.0",
	),
	basProvider: optionalObj(
		v.variant("type", [
			v.object({
				type: v.literal("file"),
				path: path,
			}),
			v.object({
				type: v.literal("url"),
				path: urlOrIp,
			}),
		]),
		undefined,
	),
	ftdbProvider: v.optional(
		v.variant("type", [
			v.object({
				type: v.literal("file"),
				path: path,
			}),
			v.object({
				type: v.literal("url"),
				path: urlOrIp,
			}),
		]),
	),
	remote: optionalObj(
		v.object({
			hostname: stringWithVariables,
			port: v.optional(v.pipe(v.number(), v.integer())),
			username: v.optional(stringWithVariables),
			keyFile: v.optional(path),
		}),
		undefined,
	),
	sourceRepo: v.optional(
		v.variant("type", [
			v.object({
				type: v.literal("local"),
				sourceRoot: v.optional(path),
			}),
			v.object({
				type: v.literal("sftp"),
				sourceRoot: v.optional(path),
				hostname: stringWithVariables,
				port: v.optional(v.pipe(v.number(), v.integer())),
				username: v.optional(stringWithVariables),
				keyFile: v.optional(path),
			}),
			v.object({
				type: v.literal("p4"),
				server: urlOrIp,
				mappings: v.array(P4Mapping),
				cleanSourceList: urlOrFile,
				intermediateArchive: archiveSource,
			}),
			v.object({
				type: v.literal("repo"),
				server: url,
				branch: v.optional(stringWithVariables),
				mappings: v.array(
					v.object({
						rootPath: path,
						targetPath: path,
					}),
				),
				intermediateArchive: archiveSource,
			}),
			v.object({
				type: v.literal("archive"),
				source: archiveSource,
				archiveFormat: v.picklist(["zip", "tar"]),
				intermediateArchive: archiveSource,
			}),
		]),
		{
			type: "local",
		},
	),
	opengrok: v.optional(
		v.object({
			url: url,
			apiKey: stringWithVariables,
		}),
	),
	snippets: v.optional(
		v.array(
			v.object({
				name: v.string(),
				command: v.string(),
			}),
		),
	),
});

export type Manifest = v.InferOutput<typeof ManifestSchema>;

export const WorkspaceSchema = v.object({
	settings: v.looseObject({
		"cas.manifest": ManifestSchema,
	}),
	folders: v.array(
		v.object({
			path: v.string(),
		}),
	),
});
export type WorkspaceFile = v.InferOutput<typeof WorkspaceSchema>;
