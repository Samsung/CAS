import { basename } from "node:path";
import { withAbort } from "@cas/helpers";
import {
	allResults,
	anyResult,
	Err,
	EventualResult,
	None,
	Ok,
	type Option,
	type Result,
	Some,
} from "@opliko/eventual-result";
import {
	type CancellationToken,
	commands,
	type ExtensionContext,
	FileType,
	type Progress,
	ProgressLocation,
	Uri,
	type WorkspaceFolder,
	window,
	workspace,
} from "vscode";
import type { Logger } from "winston";
import { findDepsFiles, loadDeps } from "./deps";
import { getLogger, quiet } from "./logger";
import { Mapping } from "./mapping/mapping";
import { ClientStatus, P4 } from "./p4";
import settings from "./settings";
import { MappingDecorationProvider } from "./source-control/decorations";
import { P4SourceControlProvider } from "./source-control/source-control-provider";
import { confirm } from "./utils";

interface WorkspaceCommandOptions {
	progress?: Progress<{ message?: string; increment?: number }>;
	cancel?: CancellationToken;
	signal?: AbortSignal;
}
interface WorkspaceCreationOptions extends WorkspaceCommandOptions {
	initial?: boolean;
	confirm?: boolean;
	useArchive?: boolean;
}

export class Main implements Disposable {
	//#region props
	#context: ExtensionContext;
	#mapping?: Mapping;
	#deps?: Uri;
	#p4: P4;
	#workspaceName?: string;
	abort: AbortController;

	#logger: Logger;

	// providers
	#mappingDecorationProvider: MappingDecorationProvider;
	#scm?: P4SourceControlProvider;

	//#endregion

	//#region initialization
	constructor(context: ExtensionContext) {
		this.#logger = getLogger("main");
		this.#context = context;
		this.#mappingDecorationProvider = new MappingDecorationProvider(undefined);
		this.#p4 = new P4();
		this.abort = new AbortController();
		this.registerSubscriptions();
		// new EventualResult(this.workspaceFolder()).effect((folder) => {
		// 	this.#scm = new P4SourceControlProvider(folder.uri);
		// });
	}
	async initialChecks() {
		if (!settings.isP4Workspace) {
			await new Promise<void>((resolve) =>
				settings.watch("type", (type) => type === "p4" && resolve()),
			);
		}
		await new EventualResult(this.depsFile())
			.effect((file) => {
				this.#logger.debug(`found deps file: ${file}`);
			})
			.effectErr(async (error) => {
				this.#logger.error("no deps file", { error });
				await commands.executeCommand(
					"setContext",
					"cas-p4.depsDetected",
					false,
				);
			})
			.andThen((_) =>
				window.withProgress(
					{
						location: ProgressLocation.Notification,
						cancellable: true,
						title: "Creating P4 workspace",
					},
					(progress, cancel) =>
						this.createWorkspace({
							progress,
							cancel,
							confirm: !settings.get<boolean>("skipConfirm", false),
							initial: true,
						}),
				),
			);
	}
	registerSubscriptions() {
		const fsWatcher = workspace.createFileSystemWatcher("**/deps.json");
		fsWatcher.onDidCreate(() =>
			window.withProgress(
				{
					location: ProgressLocation.Notification,
					cancellable: true,
					title: "Creating P4 workspace",
				},
				(progress, cancel) =>
					this.createWorkspace({
						progress,
						cancel,
						confirm: true,
						initial: false,
					}),
			),
		);
		fsWatcher.onDidChange(() =>
			window.withProgress(
				{
					location: ProgressLocation.Notification,
					cancellable: true,
					title: "Creating P4 workspace",
				},
				(progress, cancel) =>
					this.createWorkspace({
						progress,
						cancel,
						confirm: true,
						initial: false,
					}),
			),
		);
		fsWatcher.onDidDelete(() =>
			window.withProgress(
				{
					location: ProgressLocation.Notification,
					cancellable: false,
					title: "Deleting P4 workspace",
				},
				(progress, cancel) => this.deleteWorkspace({ progress, cancel }),
			),
		);
		this.#context.subscriptions.push(
			window.registerFileDecorationProvider(this.#mappingDecorationProvider),
			commands.registerCommand("cas-p4.setMappingTemplatePath", () =>
				this.setMappingTemplatePath(),
			),
			commands.registerCommand("cas-p4.fetchWorkspace", () =>
				window.withProgress(
					{
						location: ProgressLocation.Notification,
						cancellable: true,
						title: "Creating P4 workspace",
					},
					(progress, cancel) =>
						this.createWorkspace({
							progress,
							cancel,
							confirm: false,
							initial: false,
						}),
				),
			),
			commands.registerCommand("cas-p4.deleteWorkspace", () =>
				window.withProgress(
					{
						location: ProgressLocation.Notification,
						cancellable: false,
						title: "Deleting P4 workspace",
					},
					(progress, cancel) => this.deleteWorkspace({ progress, cancel }),
				),
			),
			commands.registerCommand("cas-p4.sync", () =>
				window.withProgress(
					{
						location: ProgressLocation.Notification,
						cancellable: false,
						title: "Deleting P4 workspace",
					},
					(progress, cancel) => this.syncWorkspace({ progress, cancel }),
				),
			),
			commands.registerCommand("cas-p4.openFile", (file) =>
				this.openFile(file),
			),
			fsWatcher,
		);
	}

	setupEvents() {
		// this.#mapping?.newMappings.event((uri) => this.#scm?.addToGroup(uri));
		this.#p4.synchronized.event(() => this.#scm?.redecorate());
		this.#mapping?.newMappings.event(() => {
			if (this.#mapping) {
				this.#mappingDecorationProvider.mapping = this.#mapping;
			}
		});
		this.#p4.synchronized.event(() => {
			if (this.#mapping) {
				this.#mappingDecorationProvider.mapping = this.#mapping;
			}
		});
	}

	setupScm() {
		this.setupEvents();
	}

	async loadMapping(reload = false): Promise<Result<Mapping, string>> {
		if (!settings.mappingTemplatePath) {
			return new Err("Template path not set");
		}
		if (this.#mapping && !reload) {
			return new Ok(this.#mapping);
		}
		return new EventualResult(this.workspaceName())
			.andThen((root) =>
				Mapping.loadTemplateFromSettings({
					context: this.#context,
					root: `/${root}`,
				}),
			)
			.andThen(async (mapping) =>
				new EventualResult(
					mapping.loadCleanList(
						settings.cleanListURL ??
							Uri.joinPath(
								workspace.workspaceFolders?.at(0)?.uri ?? Uri.file("./"),
								"clean_list.json",
							),
					),
				)
					.map(() => mapping)
					.effectErr((err) => {
						this.#logger.error(err);
					}),
			)
			.effect((mapping) => {
				this.#mapping = mapping;
			})
			.effect(() => this.setupEvents());
	}
	//#endregion

	//#region vscode workspace

	/**
	 * Get the "main" deps.json file to be used by this extension
	 * Should return the file closest to the workspace root, even if there are multiple of them
	 * @returns primary deps.json file used by this extension
	 */
	async depsFile(): Promise<Result<Uri, unknown[]>> {
		const deps = await findDepsFiles();

		return anyResult(
			(
				await Promise.allSettled(
					deps.map((dep) =>
						workspace.fs
							.stat(dep)
							.then((stat) =>
								stat.type !== FileType.Directory ? dep : undefined,
							),
					),
				)
			)
				.sort(
					// sort by shortest path first and fulfilled first
					(a, b) =>
						a.status === "fulfilled" && b.status === a.status
							? (a.value?.fsPath.length ?? 0) - (b.value?.fsPath.length ?? 0)
							: b.status.length - a.status.length,
				)
				.map((result: PromiseSettledResult<Uri | undefined>) => {
					if (result.status === "fulfilled" && result.value) {
						return new Ok(result.value);
					}
					if (result.status === "fulfilled") {
						return new Err("No result");
					}
					return new Err(result.reason?.toString());
				}),
		) as Result<Uri, unknown[]>;
	}

	async workspaceFolder(): Promise<Result<WorkspaceFolder, string>> {
		return (this.#deps ? new Ok(this.#deps) : await this.depsFile())
			.map((deps) => workspace.getWorkspaceFolder(deps))
			.andThen((folder) =>
				folder ? new Ok(folder) : new Err(["Workspace folder not found"]),
			)
			.mapErr((err) => {
				this.#logger.error("Failed loading workspace folder, reasons:", {
					err: err,
				});
				return err.join();
			}) as Result<WorkspaceFolder, string>;
	}

	async workspaceName(): Promise<Result<string, string>> {
		return workspace.workspaceFile
			? new Ok(
					basename(workspace.workspaceFile.fsPath, ".code-workspace").replace(
						/[^\w\d\.\-]/g,
						"_",
					),
				)
			: new EventualResult(this.workspaceFolder()).map((folder) =>
					folder.name.replace(/[^\w\d\.\-]/g, "_"),
				);
	}
	//#endregion

	//#region commands

	async openFile(file: Uri) {
		if (window.activeTextEditor?.document.uri === file) {
			return;
		}
		const doc = await workspace.openTextDocument(file);
		await window.showTextDocument(doc);
	}

	async setMappingTemplatePath() {
		const path = await window.showOpenDialog({
			canSelectMany: false,
			filters: {
				JSON: ["json"],
			},
			title: "Select mapping template file",
			openLabel: "Select",
		});
		if (!path || !path.length) {
			this.#logger.debug("no file selected");
			return;
		}
		const selectedPath = path[0].fsPath;
		if (settings.mappingTemplatePath !== selectedPath) {
			settings.mappingTemplatePath = selectedPath;
			this.#logger.info(`changed mapping template path to ${selectedPath}`);
		} else {
			this.#logger.info(`template path was already set to ${selectedPath}`);
		}
	}
	@resetSignal
	async syncWorkspace({
		signal: _signal,
		progress: _,
	}: WorkspaceCommandOptions) {
		return allResults<Mapping | string | string[], string>(
			(
				await Promise.all([
					new EventualResult(this.loadMapping()).andThen(
						async (mapping) =>
							allResults<Mapping | string[], string>([
								new Ok(mapping),
								await new EventualResult(this.depsFile())
									.andThen(async (depsFile) =>
										mapping.createMappings(depsFile, settings.archiveURL),
									)
									.mapErr((e) => e.join("\n")),
							]) as Result<[Mapping, string[]], string>,
					),
					this.workspaceName(),
				])
			).flat() as Result<Mapping | string[] | string, string>[],
		).eventually();
		// .andThen(async (values) => {
		// 	const mapping = values[0] as Mapping;
		// 	const mappings = values[1] as string[];
		// 	const name = values[2] as string;
		// 	return this.#p4.sync(
		// 		name,
		// 		mapping.changelist,
		// 		mapping.partials,
		// 		true,
		// 		mappings,
		// 		progress,
		// 		signal,
		// 	);
		// })
		// .effectErr((err) => {
		// 	this.#logger.error(`failed synchronizing workspace: ${err}`);
		// });
	}
	@resetSignal
	async createWorkspace(
		creationOptions: WorkspaceCreationOptions,
	): Promise<Result<unknown, unknown>> {
		await commands.executeCommand("setContext", "cas-p4.depsDetected", true);
		creationOptions.cancel?.onCancellationRequested(this.deleteWorkspace);
		const { progress, signal } = creationOptions;
		progress?.report({
			increment: 0,
			message: "",
		});
		return new EventualResult(
			withAbort(
				allResults<ClientStatus | string[] | string, unknown>(
					await Promise.race([
						new Promise<[Result<never, string>]>((resolve) =>
							signal?.addEventListener("abort", () =>
								resolve([new Err("aborted")]),
							),
						),
						Promise.allSettled([
							new EventualResult(this.workspaceName())
								.effect((name) => {
									this.#workspaceName = name;
								})
								.andThen((name) =>
									new EventualResult(this.workspaceFolder()).map((folder) => ({
										name,
										root: folder.uri,
									})),
								)
								.map((wsData) => this.#p4.exists(wsData))
								.effect((status) => {
									this.#logger.debug(`workspace status: ${status}`);
								})
								.andThen<Option<ClientStatus>, unknown>((status) => {
									if (!creationOptions.confirm) {
										return new Ok(new Some(status));
									}
									if (signal?.aborted) {
										return new Err("aborted");
									}
									switch (status) {
										case ClientStatus.Exists: {
											if (creationOptions.initial) {
												return new Ok(None);
											}
											return confirm(
												"Changes in deps.json detected, do you want to update the P4 workspace?",
												status,
												signal,
											);
										}
										case ClientStatus.DifferentRoot: {
											return confirm(
												"Detected deps.json, but relevant P4 workspace exists and has a different root. Do you want to replace it?",
												status,
												signal,
											);
										}
										case ClientStatus.DoesNotExist: {
											return confirm(
												`Detected ${!creationOptions.initial ? "changes in " : ""}deps.json, do you want to create a P4 workspace?`,
												status,
												signal,
											);
										}
									}
								})
								.andThen((v) =>
									v.okOrElse(() => {
										this.#logger.info("operation cancelled by user");
									}),
								)
								.mapErr(() => quiet)
								.effect(() => {
									this.#logger.debug(
										`${creationOptions.confirm ? "user consent granted, " : ""}proceeding with workspace creation`,
									);
								}),
							new EventualResult(this.loadMapping(true))
								.effect(() => {
									this.#logger.debug("loaded mapping template");
								})
								.effectErr((error) => {
									if (error) {
										this.#logger.error("failed loading mapping template", {
											error,
										});
									}
								})
								.mapErr(() => quiet)
								.effect((mapping) => {
									this.#mapping = mapping;
								})
								.effect(() => {
									progress?.report({
										message: "Creating mappings",
										increment: 10,
									});
								})
								.andThen(async (mapping) =>
									new EventualResult(this.depsFile()).andThen(
										async (depsFile) =>
											mapping.createMappings(depsFile, settings.archiveURL),
									),
								)
								.andThen<string[], string>((v) =>
									signal?.aborted ? new Err("aborted") : new Ok(v),
								)
								.effect(() => {
									this.#logger.debug("created mappings");
								})
								.effect(() => {
									progress?.report({
										message: "Loading P4 workspace data",
										increment: 10,
									});
								}),
						]).then((promises) =>
							promises.map((promise) =>
								promise.status === "fulfilled" ? promise.value : promise.reason,
							),
						),
					]),
				)
					.eventually()
					.mapErr((e) => {
						if (typeof e === "string") {
							this.#logger.info(
								`failed setting up for workspace creation: ${e}`,
							);
						} else {
							this.#logger.info("failed setting up for workspace creation");
							this.#logger.log("trace", e);
						}
						return quiet;
					})
					.andThen(async (args) => {
						const [status, mappings] = args;
						if (signal?.aborted) {
							return new Err("aborted");
						}
						if (!Array.isArray(mappings)) {
							return new Err(mappings);
						}
						if (status === ClientStatus.Exists && creationOptions.initial) {
							if (
								!this.#mapping?.archiveMap ||
								!(await this.#mapping.archiveMap.remaining()).size ||
								(!this.#context.workspaceState.get<boolean>(
									"archiveExtracted",
									false,
								) &&
									!(
										await confirm(
											"Workspace exists, but there are still some paths missing, do you want to try downloading them from intermediates archive?",
											"archive",
										)
									)
										.unwrap()
										.unwrapOr(""))
							) {
								this.#logger.info(
									"workspace already exists, aborting creation on initial run",
								);
								return new Err(false);
							}
							return new Ok([] as (string | string[])[]);
						}
						return allResults<string[] | string, unknown>([
							new Ok(mappings),
							await this.#p4.createWorkspace(
								this.#workspaceName ?? (await this.workspaceName()).unwrap(),
								(await this.workspaceFolder()).unwrap().uri,
								mappings,
								this.#mapping!.mappedTrie!,
								this.#mapping?.mappings.reduce(
									(cl, mapping) => Math.max(mapping.clSync, cl),
									0,
								),
								this.#mapping?.mappings.flatMap((mapping) => mapping.clPartial),
								progress,
								signal,
							),
						]);
					})
					.effect(() => {
						progress?.report({
							increment: 10,
							message: "Extracting missing files from archive",
						});
					})
					.andThen(async () => {
						if (
							this.#mapping?.archiveMap &&
							(creationOptions.useArchive ?? true)
						) {
							return this.#mapping?.archiveMap?.extractPaths(
								(await this.workspaceFolder()).unwrap().uri.fsPath,
								signal,
								progress,
							);
						}
						return new Ok(0);
					})
					.effect(() => {
						progress?.report({ increment: 10 });
					})
					.mapErr((error) => {
						if (error) {
							this.#logger.error("Failed extracting from archive", { error });
						}
						return error;
					})
					.effect(() => {
						this.#logger.info("Created and synchronized P4 workspace");
					}),
				signal,
			),
		);
	}

	@resetSignal
	async deleteWorkspace(options: WorkspaceCommandOptions) {
		const { progress, signal } = options;
		progress?.report({
			message: "Deleting P4 workspace",
			increment: 0,
		});
		return new EventualResult(this.workspaceName())
			.effect((name) => {
				this.#workspaceName = name;
			})
			.andThen((name) =>
				new EventualResult(this.workspaceFolder()).map((folder) => ({
					name,
					root: folder.uri,
				})),
			)
			.map((wsData) => this.#p4.exists(wsData))
			.andThen<Option<ClientStatus>, unknown>((status) => {
				if (signal?.aborted) {
					return new Err("aborted");
				}
				switch (status) {
					case ClientStatus.DifferentRoot: {
						return confirm(
							`workspace ${this.#workspaceName} exists, but has different root folder. Are you sure you want to delete it?`,
							status,
							signal,
						);
					}
					case ClientStatus.Exists: {
						return confirm(
							`Do you want to delete the P4 workspace ${this.#workspaceName}?`,
							status,
							signal,
						);
					}
					default: {
						return new Err("Workspace not found");
					}
				}
			})
			.andThen((result) =>
				result.okOrElse(() => this.#logger.info("Deletion cancelled")),
			)
			.andThen(async () =>
				this.#p4.delete(
					this.#workspaceName ?? (await this.workspaceName()).unwrap(),
					signal,
				),
			)
			.effect(
				async () =>
					await commands.executeCommand(
						"setContext",
						"cas-p4.depsDetected",
						false,
					),
			)
			.effectErr((error) => {
				this.#logger.error("failed deleting workspace: ", { error });
			});
	}

	//#endregion

	[Symbol.dispose](): void {
		if (this.#mapping) {
			this.#mapping[Symbol.dispose]();
		}
	}
}

function resetSignal<
	This extends InstanceType<typeof Main>,
	Args extends [WorkspaceCommandOptions],
	Return,
>(
	method: (this: This, ...args: Args) => Return,
	_context: ClassMethodDecoratorContext<
		This,
		(this: This, ...args: Args) => Return
	>,
) {
	return function replacement(this: This, ...args: Args) {
		this.abort.abort();
		this.abort = new AbortController();
		args[0].signal = this.abort.signal;
		return method.call(this, ...args);
	};
}
