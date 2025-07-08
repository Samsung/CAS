import { DepSchema, DepsSchema } from "@cas/deps";
import { encodeText, withTrailingSlash } from "@cas/helpers";
import { readJSON } from "@cas/helpers/vscode/fs.js";
import { BASFile } from "@cas/types/bas.js";
import { Paged } from "@cas/types/cas_server.js";
import { existsSync, readdirSync, rmdirSync } from "fs";
import { mkdir, symlink } from "fs/promises";
import { dirname, join, normalize, parse, relative } from "path";
import { FileSystemError, Uri, workspace } from "vscode";
import { debug, error, info } from "../../logger";
import { DepsGenerator, DepsGeneratorOptions } from "./generator";

const { fs } = workspace;

function toDepsPathSet(
	deps: DepsSchema,
	sourceRoot: string,
	workspaceDir: string,
): Set<string> {
	return new Set(
		deps.entries
			.filter((entry: DepSchema) => entry.original_path.startsWith(sourceRoot))
			.map((dep) =>
				dep.original_path.startsWith(sourceRoot)
					? join(workspaceDir, relative(sourceRoot, dep.original_path))
					: dep.original_path,
			),
	);
}

const localGenerator: DepsGenerator = {
	createSteps: 1,
	async createDeps({ out, reportProgress, deps, sourceRoot, binaries }) {
		reportProgress?.({ message: "Creating symlinks" });
		let symlinkCount = 0;
		let skipCount = 0;
		const depsRoots = new Set<string>();
		info(`deps.entries ${deps.entries.length} `);
		deps.entries = (
			await Promise.allSettled(
				deps.entries
					.filter((entry: DepSchema) =>
						entry.original_path.startsWith(sourceRoot),
					)
					.map(async (entry: DepSchema) => {
						entry.binary = binaries.includes(entry.original_path);
						entry.original_path = normalize(entry.original_path);
						entry.workspace_path = entry.original_path.startsWith(sourceRoot)
							? join(out, relative(sourceRoot, entry.original_path))
							: entry.original_path; //out of source directory - original path
						await mkdir(dirname(entry.workspace_path), { recursive: true });
						await symlink(entry.original_path, entry.workspace_path, "file")
							.then((_) => {
								symlinkCount++;
								debug(
									`[generator.local] createDeps() ADDED link ${entry.original_path} -> ${entry.workspace_path}`,
								);
							})
							.catch((err: FileSystemError) => {
								skipCount++;
								if (err.code !== "EEXIST") {
									error(
										`[generator.local] (${entry.original_path}) ${err.message}`,
										true,
									);
									throw err;
								}
							});
						depsRoots.add(
							join(
								out,
								entry.filename.split(/\/|\\/).filter(Boolean).at(0) ?? "",
							),
						);
						return entry;
					}),
			)
		)
			.filter((result) => result.status === "fulfilled")
			.map((result) => result.value);

		info(
			`[generator.local] Linked files new=${symlinkCount} skipped=${skipCount}`,
		);
		deps.count = deps.entries.length;
		deps.num_entries = deps.entries.length;
		deps.version = 2;
		return {
			deps,
			depsFolders: [...depsRoots],
		};
	},
	updateSteps: 1,
	async update({ out, deps, sourceRoot }) {
		const oldDeps = await readJSON<DepsSchema>(join(out, "deps.json"));
		const newSet = toDepsPathSet(deps, sourceRoot, out);
		const removedEntries = new Set(
			oldDeps.entries.map((dep) => dep.workspace_path),
		).difference(newSet);
		await Promise.all(
			[...removedEntries.values()].map((path) => {
				if (path?.startsWith(out)) {
					return fs.delete(Uri.file(path)).then(
						(_done) => {
							debug(`[generator.local] REMOVED ${path}`);
							let dirPath = dirname(path);
							while (existsSync(dirPath) && readdirSync(dirPath).length === 0) {
								debug(`[generator.local] REMOVED empty dir ${dirPath}`);
								let ndirPath = dirname(dirPath);
								rmdirSync(dirPath);
								dirPath = ndirPath;
							}
						},
						(_err) => {},
					);
				}
			}),
		);
	},
};

export default localGenerator;
