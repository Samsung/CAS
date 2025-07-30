import { DepSchema } from "@cas/deps";
import { join, normalize, relative } from "path";
import { DepsGenerator } from "./generator";

const p4Generator: DepsGenerator = {
	createSteps: 1,
	async createDeps({ out, reportProgress, deps, sourceRoot, binaries }) {
		reportProgress?.({ message: "Creating deps.json" });
		deps.entries = deps.entries
			.filter((entry: DepSchema) => entry.original_path.startsWith(sourceRoot))
			.map((entry) => {
				entry.binary = binaries.includes(entry.original_path);
				entry.original_path = normalize(entry.original_path);
				entry.workspace_path = entry.original_path.startsWith(sourceRoot)
					? join(out, relative(sourceRoot, entry.original_path))
					: entry.original_path; //out of source directory - original path
				return entry;
			});
		const depsRoots = new Set<string>(
			deps.entries.map((entry) => {
				return join(
					out,
					entry.filename.split(/\/|\\/).filter(Boolean).at(0) ?? "",
				);
			}),
		);

		deps.count = deps.entries.length;
		deps.num_entries = deps.entries.length;
		deps.version = 2;
		return {
			deps,
			depsFolders: [...depsRoots],
			additionalManifestData: {
				settings: {
					"cas-p4.skipConfirm": true,
				},
			},
		};
	},
	updateSteps: 0,
	async update() {},
};

export default p4Generator;
