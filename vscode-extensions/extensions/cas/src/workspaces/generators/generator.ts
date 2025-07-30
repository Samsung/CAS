import { DepsSchema } from "@cas/deps";
import type { MaybePromise } from "@cas/helpers/promise.ts";
import { Manifest } from "@cas/manifest";
import { ExtensionContext, Progress } from "vscode";
import { DBProvider } from "../../db";
import { Settings } from "../../settings";

export interface DepsGeneratorOptions {
	cmd: string;
	paths: string[];
	out: string;
	reportProgress?: Progress<{
		message?: string;
		increment?: number;
	}>["report"];
	sourceRoot: string;
	deps: DepsSchema;
	binaries: string[];
	ctx: ExtensionContext;
	dbProvider: DBProvider;
	settings: Settings;
	manifest?: Manifest;
	update: boolean;
}
export interface DepsGeneratorResult {
	deps: DepsSchema;
	depsFolders: string[];
	additionalManifestKeys?: (keyof Manifest)[];
	additionalManifestData?: Record<string, any>;
}
export interface DepsGenerator {
	createDeps(options: DepsGeneratorOptions): MaybePromise<DepsGeneratorResult>;
	createSteps?: number;
	update(Options: DepsGeneratorOptions): MaybePromise<void>;
	updateSteps?: number;
}
