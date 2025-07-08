import { DepSchema } from "@cas/deps";
import { readDeps } from "@cas/deps/helpers.js";
import { sleep } from "@cas/helpers/promise.js";
import { EventualResult } from "@opliko/eventual-result";
import { Uri, workspace } from "vscode";
import { getLogger } from "./logger";
import { commonPathPrefix } from "./mapping/paths";
import settings from "./settings";
import { readJSONWithSchema } from "./utils";

/**
 * Load all (plain-file) filenames from a deps.json file
 * @param path to a deps.json file to load
 * @returns list of filenames
 */
export async function loadDeps(path: Uri | string) {
	const uriPath = path instanceof Uri ? path : Uri.file(path);

	const logger = getLogger("Deps");

	logger.debug(`loading deps.json from ${uriPath}`);

	return new EventualResult(readDeps(uriPath.fsPath))
		.map((deps) => {
			const files = deps.entries.map((dep: DepSchema) => dep.filename);
			return {
				files,
				root: commonPathPrefix(
					deps.entries.map((dep) => dep.original_path),
				).slice(0, -commonPathPrefix(files).length),
			};
		})
		.mapErr((err) => {
			logger.error(err);
			return `failed loading dependencies: ${err}`;
		});
}

/**
 * Find all deps.json files to load
 * @returns list of Uris for
 * ! for some reasons calls to workspace.findFiles can return no results eevn when there are matching files
 * ! that's why this function retries the search a few times
 */
export async function findDepsFiles() {
	const logger = getLogger("deps");
	// TODO: support some nesting or multiple workspaces perhaps?
	for (let i = 0; i < settings.maxDepsSearchAttempts; i++) {
		const files = await workspace.findFiles("deps.json");
		logger.debug("deps.json files", { files });
		if (files.length) {
			return files;
		}
		// simple exponential backoff for retries
		await sleep(8 ** i);
	}
	logger.warn(
		"deps.json not found in workspace, attempting to use default path anyway",
	);
	return [Uri.file("deps.json")];
}
