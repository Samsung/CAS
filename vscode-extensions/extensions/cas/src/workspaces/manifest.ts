import {
	Manifest,
	ManifestSchema,
	WorkspaceFile,
	WorkspaceSchema,
} from "@cas/manifest";
import { toJsonSchema } from "@valibot/to-json-schema";
import * as v from "valibot";
import {
	commands,
	EventEmitter,
	ExtensionContext,
	Uri,
	window,
	workspace,
} from "vscode";
import { DBInfo, DBProvider } from "../db/index";
import { Snippets } from "../db/snippets";
import { debug, error, warn } from "../logger";
import { Settings } from "../settings";

export class ManifestSettings {
	#manifest: Manifest | undefined = undefined;
	#context: ExtensionContext;
	#settings: Settings;
	#db: DBProvider;
	#parsedManifestEventEmitter = new EventEmitter<Manifest | undefined>();
	#snippets: Snippets;
	public onParsedManifest = this.#parsedManifestEventEmitter.event;
	#initDone = new EventEmitter<void>();
	public onInitDone = this.#initDone.event;
	constructor(context: ExtensionContext, settings: Settings, db: DBProvider) {
		this.#context = context;
		this.#settings = settings;
		this.#db = db;
		this.#snippets = Snippets.init(context);

		context.subscriptions.push(
			commands.registerCommand("cas.manifest.schema", async () => {
				const schema = toJsonSchema(WorkspaceSchema, {
					errorMode: "warn",
				});
				await window.showTextDocument(
					await workspace.openTextDocument({
						language: "json",
						content: JSON.stringify(schema, undefined, 2),
					}),
				);
			}),
		);
		this.#settings.listen("manifest", (manifest) =>
			this.parseManifestObject(manifest),
		);
		if (this.#settings.get("manifest")) {
			setImmediate(() => {
				try {
					this.parseManifestObject(this.#settings.get("manifest"));
				} finally {
					this.#initDone.fire();
				}
			});
		}
	}

	private processParsedManifest(
		result:
			| v.SafeParseResult<typeof ManifestSchema>
			| v.SafeParseResult<typeof WorkspaceSchema>,
		original: unknown,
	) {
		if (result.success) {
			debug("successfully parsed manifest");
			const manifest =
				"folders" in result.output
					? result.output.settings["cas.manifest"]
					: result.output;
			this.#parsedManifestEventEmitter.fire(manifest);
			this.#manifest = manifest;
			this.setSettings(manifest);
			return manifest;
		}

		for (const topIssue of result.issues) {
			for (const issue of topIssue.issues ?? []) {
				debug(`${issue.type} issue found in manifest: ${issue.message}`);
				if (
					issue.path?.map((item) => item.key).join(".") === "sourceRepo.type"
				) {
					warn(
						`invalid manifest type: ${issue.input} (supported types: ${issue.issues?.map((i) => i.expected)}); trying with type:local instead`,
					);

					(original as WorkspaceFile).settings["cas.manifest"].sourceRepo = {
						type: "local",
					};
					return this.parseManifestObject(original);
				}
			}
		}
		debug(`manifest errors: ${JSON.stringify(result.issues, null, 2)}`);
		error(`failed parsing manifest`);
	}

	/**
	 *
	 * @param manifestData Object containing just the manifest to parse
	 */
	parseManifestObject(manifestData: unknown): Manifest | void {
		return this.processParsedManifest(
			v.safeParse(ManifestSchema, manifestData),
			manifestData,
		);
	}
	async setSettings(manifest: Manifest | undefined, force = false) {
		if (!manifest) {
			return;
		}
		commands.executeCommand("setContext", "cas.manifestLoaded", true);
		const scheme = `bas_${manifest?.basProvider?.type}` as
			| "bas_file"
			| "bas_url";
		const casPath =
			scheme === "bas_file" && manifest.basProvider
				? Uri.parse(manifest?.basProvider.path).fsPath
				: manifest.basProvider?.path;
		if (casPath) {
			const dbInfo: DBInfo = {
				path: casPath,
				scheme,
			};

			const databases = this.#settings.basDBs;
			if (
				!databases.some(
					(db) => db?.path === dbInfo.path && db?.scheme === dbInfo.scheme,
				)
			) {
				databases.push(dbInfo);
				this.#settings.set("basDatabases", databases);
			}
			if (!this.#settings.inspect("basDatabase")?.workspaceValue || force) {
				await this.#db.switchDB(dbInfo);
			}
		}
		for (const { name, command } of manifest.snippets ?? []) {
			this.#snippets.set(name, command);
		}
	}

	get manifest() {
		return this.#manifest;
	}
	get initialized() {
		return !!this.#manifest;
	}

	get basServer() {
		return this.#manifest?.basProvider?.path;
	}

	get isLocal() {
		return !this.#manifest || this.#manifest.sourceRepo.type === "local";
	}
	get type() {
		return this.#manifest ? this.#manifest.sourceRepo.type : undefined;
	}
	get sourceRepoType() {
		return this.#manifest?.sourceRepo.type;
	}
	get opengrok() {
		return this.#manifest?.opengrok;
	}

	workspaceManifest(whitelist: (keyof Manifest)[]) {
		return this.manifest
			? Object.fromEntries(
					Object.entries(this.manifest).filter(([key]) =>
						whitelist.includes(key as keyof Manifest),
					),
				)
			: undefined;
	}
}
