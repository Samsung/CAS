import { sleep } from "@cas/helpers/promise.js";
import QuickLRU from "quick-lru";
import * as vscode from "vscode";
import {
	ClientCapabilities,
	DocumentSelector,
	Executable,
	FeatureState,
	InitializeParams,
	LanguageClient,
	LanguageClientOptions,
	Range,
	RequestType,
	ServerCapabilities,
	ServerOptions,
	StaticFeature,
	TextDocumentIdentifier,
	Trace,
	WorkspaceEdit,
} from "vscode-languageclient/node";
import { error } from "./logger";
import { Settings } from "./settings";

const _inlineMacroKind: vscode.CodeActionKind =
	vscode.CodeActionKind.RefactorInline.append("all_macros");

export interface MacroExpansionParams {
	textDocument: TextDocumentIdentifier;
	range: Range;
}
export const MacroExpandRangeRequest = new RequestType<
	MacroExpansionParams,
	WorkspaceEdit,
	void
>("cas.MacroExpandRange");
export const MacroExpandFunctionRequest = new RequestType<
	MacroExpansionParams,
	WorkspaceEdit,
	void
>("cas.MacroExpandFunction");
//#region main inline class
export class InlineMacrosProvider implements vscode.Disposable {
	#ctx: vscode.ExtensionContext;
	client: LanguageClient;
	macroFeature: MacroExpansionFeature;
	settings: Settings;
	constructor(ctx: vscode.ExtensionContext, settings: Settings) {
		this.#ctx = ctx;
		this.settings = settings;
		const selector: vscode.DocumentSelector = [
			{ scheme: "file", language: "c" },
			{ scheme: "file", language: "cpp" },
			{ scheme: "file", language: "cuda-cpp" },
		];

		const execPath = ctx.asAbsolutePath("dist/cas_tools/macro_proc/clangd");
		const casArgs: string[] = ["--log=verbose", "--use-dirty-headers"];

		const casOutputChannel = vscode.window.createOutputChannel("CAS clangd");

		const casLsp: Executable = {
			command: execPath,
			args: casArgs,
		};
		const serverOptions: ServerOptions = casLsp;

		const clientOptions: LanguageClientOptions = {
			documentSelector: selector as DocumentSelector,
			outputChannel: casOutputChannel,
			diagnosticPullOptions: {},
			synchronize: {
				fileEvents: [
					// workspace.createFileSystemWatcher()
				],
			},
		};
		this.client = new LanguageClient("cas_lsp", serverOptions, clientOptions);
		this.client.setTrace(Trace.Verbose);
		this.macroFeature = new MacroExpansionFeature(this.client);
		this.client.registerFeature(this.macroFeature);
		this.client
			.start()
			.then(() =>
				ctx.subscriptions.push(
					vscode.workspace.registerFileSystemProvider(
						"inlined-macro",
						new InlinedMacroFSProvider(),
					),
					vscode.commands.registerCommand(
						"cas.inlineMacros.diff",
						async (edit: vscode.WorkspaceEdit, uri: vscode.Uri) => {
							const title = `Inline Macro Preview`;
							if (!edit || !uri) {
								return vscode.commands.executeCommand(
									"cas.inlineMacros.file",
									true,
								);
							}
							await vscode.workspace.applyEdit(edit);
							return vscode.commands.executeCommand(
								"vscode.diff",
								uri,
								uri.with({
									scheme: "inlined-macro",
									fragment: `${uri.fragment}#${uri.scheme}`,
								}),
								title,
							);
						},
					),
					vscode.commands.registerCommand(
						"cas.inlineMacros.file",
						async (preview = false, functions = false) => {
							const document = vscode.window.activeTextEditor?.document;
							if (document) {
								let ranges = (
									vscode.window.activeTextEditor?.selections ?? []
								).map(
									(selection) =>
										new vscode.Range(selection.start, selection.end),
								);

								if (!ranges?.length) {
									const lines = document.lineCount;
									const characters = document.lineAt(lines - 1).range.end
										.character;

									ranges.push(
										document.validateRange(
											new vscode.Range(
												new vscode.Position(0, 0),
												new vscode.Position(lines, characters),
											),
										),
									);
								}
								const edits: vscode.WorkspaceEdit[] = [];
								for (const range of ranges) {
									const edit = await (functions
										? this.macroFeature.handleExpandFunction(document, range)
										: this.macroFeature.handleExpandRange(document, range));
									edit && edits.push(edit);
								}
								const fullEdit = new vscode.WorkspaceEdit();
								const previewUri = InlinedMacroFSProvider.getPreviewUri(
									document.uri,
								);
								fullEdit.set(
									preview ? previewUri : document.uri,
									edits.flatMap((edit) => edit.get(document.uri)),
								);
								await vscode.workspace.applyEdit(fullEdit);
								if (preview) {
									switch (this.settings.get<"peek" | "diff">("macroPreview")) {
										case "peek": {
											const scm = vscode.scm.createSourceControl(
												"cas.macros",
												"Macro Expansion",
											);
											const diffProvider = new InlinedMacroQuickDiffProvider();
											const viewChange =
												vscode.window.onDidChangeActiveTextEditor((editor) => {
													if (editor?.document !== document) {
														scm.dispose();
														viewChange.dispose();
													}
												});
											scm.quickDiffProvider = diffProvider;
											diffProvider.didProvideMacro(async (uri) => {
												if (uri.fsPath === document.uri.fsPath) {
													setTimeout(
														() =>
															vscode.commands.executeCommand(
																"editor.action.dirtydiff.next",
															),
														250,
													);
												}
											});
											break;
										}
										case "diff": {
											await vscode.commands.executeCommand(
												"vscode.diff",
												previewUri,
												document.uri,
												`Inline Macro Preview`,
											);
											break;
										}
									}
								}
								return fullEdit;
							}
						},
					),
					vscode.commands.registerCommand("cas.inlineMacros.function", () =>
						vscode.commands.executeCommand(
							"cas.inlineMacros.file",
							false,
							true,
						),
					),
					vscode.commands.registerCommand(
						"cas.inlineMacros.function.preview",
						() =>
							vscode.commands.executeCommand(
								"cas.inlineMacros.file",
								true,
								true,
							),
					),
				),
			)
			.catch((err) => error(`couldn't start server: ${err}`));
	}

	async dispose() {
		await this.client.stop();
	}
}

class InlinedMacroQuickDiffProvider implements vscode.QuickDiffProvider {
	#macroProvidedEmitter = new vscode.EventEmitter<vscode.Uri>();
	didProvideMacro = this.#macroProvidedEmitter.event;
	provideOriginalResource(uri: vscode.Uri): vscode.Uri {
		this.#macroProvidedEmitter.fire(uri);
		return InlinedMacroFSProvider.getPreviewUri(uri);
	}
}
//#endregion

//#region FileSystemProvider
class InlinedMacroFSProvider implements vscode.FileSystemProvider {
	constructor() {
		vscode.workspace.onDidOpenTextDocument(async (document) => {
			if (document.uri.scheme === "inlined-macro" && !document.isClosed) {
				await document.save();
				await vscode.window.tabGroups.close(
					vscode.window.tabGroups.all.flatMap((tabGroup) =>
						tabGroup.tabs.filter(
							(tab) =>
								tab.input instanceof vscode.TabInputText &&
								tab.input.uri.scheme === "inlined-macro",
						),
					),
					true,
				);
			}
		});
	}
	public static getPreviewUri(uri: vscode.Uri) {
		return uri.with({
			scheme: "inlined-macro",
			fragment: `${uri.fragment}#${uri.scheme}`,
		});
	}
	#changeEventEmitter = new vscode.EventEmitter<vscode.FileChangeEvent[]>();
	onDidChangeFile: vscode.Event<vscode.FileChangeEvent[]> =
		this.#changeEventEmitter.event;

	// we're only using cache here to avoid a possible race condition when before diff opens the file another one is "written", hence small limit
	editedFiles = new QuickLRU<vscode.Uri, Uint8Array>({
		maxSize: 10,
	});
	translateUri(uri: vscode.Uri): vscode.Uri {
		const schemeIndex = decodeURIComponent(uri.fragment).lastIndexOf("#");
		const oldScheme = decodeURIComponent(uri.fragment).slice(schemeIndex + 1);
		return uri.with({
			scheme: oldScheme,
			fragment: uri.fragment.slice(0, schemeIndex),
		});
	}
	async readFile(uri: vscode.Uri): Promise<Uint8Array> {
		const originalUri = this.translateUri(uri);
		return (
			this.editedFiles.get(originalUri) ??
			vscode.workspace.fs.readFile(originalUri)
		);
	}
	writeFile(
		uri: vscode.Uri,
		content: Uint8Array,
		_options: { readonly create: boolean; readonly overwrite: boolean },
	): void {
		const originalUri = this.translateUri(uri);
		this.editedFiles.set(originalUri, content);
		this.#changeEventEmitter.fire([
			{ uri, type: vscode.FileChangeType.Changed },
		]);
	}
	//#endregion
	//#region unimplemented
	watch(
		_uri: vscode.Uri,
		_options: {
			readonly recursive: boolean;
			readonly excludes: readonly string[];
		},
	): vscode.Disposable {
		throw new Error("Method not implemented.");
	}
	stat(uri: vscode.Uri): vscode.FileStat | Thenable<vscode.FileStat> {
		return vscode.workspace.fs.stat(this.translateUri(uri));
	}
	readDirectory(
		uri: vscode.Uri,
	): [string, vscode.FileType][] | Thenable<[string, vscode.FileType][]> {
		return vscode.workspace.fs.readDirectory(this.translateUri(uri));
	}
	createDirectory(uri: vscode.Uri): void | Thenable<void> {
		return vscode.workspace.fs.createDirectory(this.translateUri(uri));
	}

	delete(
		_uri: vscode.Uri,
		_options: { readonly recursive: boolean },
	): void | Thenable<void> {
		throw new Error("Method not implemented.");
	}
	rename(
		_oldUri: vscode.Uri,
		_newUri: vscode.Uri,
		_options: { readonly overwrite: boolean },
	): void | Thenable<void> {
		throw new Error("Method not implemented.");
	}
	copy?(
		_source: vscode.Uri,
		_destination: vscode.Uri,
		_options: { readonly overwrite: boolean },
	): void | Thenable<void> {
		throw new Error("Method not implemented.");
	}
	//#endregion
}

//#region MacroExpansionFeature
class MacroExpansionFeature implements StaticFeature {
	client: LanguageClient;
	constructor(client: LanguageClient) {
		this.client = client;
	}

	async handleExpandRange(
		document: vscode.TextDocument,
		range: vscode.Range,
	): Promise<vscode.WorkspaceEdit> {
		let doneInlining = false;
		const inliningTimer = setTimeout(async () => {
			if (!doneInlining) {
				vscode.window.withProgress(
					{
						location: vscode.ProgressLocation.Notification,
						title: "Inlining Macros",
					},
					async ({ report }, token) => {
						while (!doneInlining && !token.isCancellationRequested) {
							report({ increment: 1 });
							await sleep(100);
						}
					},
				);
			}
		}, 1000);
		const params: MacroExpansionParams = {
			textDocument: { uri: document.uri.toString() },
			range: { start: range.start, end: range.end },
		};
		const result: WorkspaceEdit = await this.client.sendRequest(
			MacroExpandRangeRequest,
			params,
		);

		let edit: vscode.WorkspaceEdit = new vscode.WorkspaceEdit();
		if (result.changes) {
			result.changes[params.textDocument.uri].forEach((e) => {
				edit.replace(document.uri, e.range as vscode.Range, e.newText);
			});
		}
		doneInlining = true;
		clearTimeout(inliningTimer);

		return edit;
	}
	async handleExpandFunction(
		document: vscode.TextDocument,
		range: vscode.Range,
	) {
		const actions: vscode.CodeAction[] = await vscode.commands.executeCommand(
			"vscode.executeCodeActionProvider",
			document.uri,
			range,
			vscode.CodeActionKind.Refactor.value,
		);
		let expandAction;
		if (
			actions.length &&
			(expandAction = actions.find(
				(action) => action.title === "Expand selected function body",
			)) &&
			expandAction.command &&
			expandAction.command.arguments
		) {
			try {
				const preApply = document.getText();
				await vscode.commands.executeCommand(
					expandAction.command.command,
					...expandAction.command.arguments,
				);
				const revert = new vscode.WorkspaceEdit();
				const postApply = document.getText();
				revert.replace(
					document.uri,
					document.validateRange(new vscode.Range(0, 0, Infinity, Infinity)),
					preApply,
				);
				await vscode.workspace.applyEdit(revert);
				const edit = new vscode.WorkspaceEdit();
				edit.replace(
					document.uri,
					document.validateRange(new vscode.Range(0, 0, Infinity, Infinity)),
					postApply,
				);
				return edit;
			} catch {}
		}
		const collapseRanges: vscode.FoldingRange[] =
			await vscode.commands.executeCommand(
				"vscode.executeFoldingRangeProvider",
				document.uri,
			);
		const legend: vscode.SemanticTokensLegend =
			await vscode.commands.executeCommand(
				"vscode.provideDocumentSemanticTokensLegend",
				document.uri,
			);
		const tokens: vscode.SemanticTokens = await vscode.commands.executeCommand(
			"vscode.provideDocumentSemanticTokens",
			document.uri,
		);
		if (!legend || !tokens) {
			return undefined;
		}
		const localRanges: vscode.FoldingRange[] = (
			await Promise.all(
				collapseRanges
					.filter(
						(collapseRange) =>
							(!collapseRange.kind || collapseRange.kind === 3) &&
							new vscode.Range(
								collapseRange.start,
								0,
								collapseRange.end,
								Infinity,
							).contains(range),
					)
					.map(async (foldingRange) => {
						// function signature should be at the start of the range
						const signatureRange = new vscode.Range(
							foldingRange.start - 2,
							0,
							foldingRange.start + 1,
							0,
						);
						let pos = new vscode.Position(0, 0);
						for (let i = 0; i < tokens.data.length; i += 5) {
							pos = new vscode.Position(
								pos.line + tokens.data[i],
								(tokens.data[0] > 0 ? pos.character : 0) + tokens.data[i + 1],
							);
							if (
								signatureRange.contains(pos) &&
								legend.tokenTypes[tokens.data[i + 3]] === "function"
							) {
								return foldingRange;
							}
						}
						return undefined;
					}),
			)
		).filter((x) => x !== undefined);
		let edit: vscode.WorkspaceEdit = new vscode.WorkspaceEdit();

		for (const localRange of localRanges) {
			const range = document.validateRange(
				new vscode.Range(localRange.start, 0, localRange.end, Infinity),
			);

			const rangeEdit = await this.handleExpandRange(document, range);
			edit.set(document.uri, [
				...edit.get(document.uri),
				...rangeEdit.get(document.uri),
			]);
		}
		return edit;
	}
	fillClientCapabilities(_capabilities: ClientCapabilities) {}
	fillInitializeParams(_params: InitializeParams) {}

	initialize(
		_capabilities: ServerCapabilities,
		_documentSelector: vscode.DocumentSelector | undefined,
	): void {
		vscode.commands.executeCommand(
			"setContext",
			"cas.MacroExpansion.supported",
			true,
			// "memoryUsageProvider" in capabilities,
		);
	}
	getState(): FeatureState {
		return { kind: "static" };
	}

	clear(): void {
		throw new Error("Method not implemented.");
	}
}
//#endregion
