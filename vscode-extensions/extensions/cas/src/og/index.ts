import { withLeadingSlash, withoutLeadingSlash } from "@cas/helpers";
import { http } from "@cas/http";
import { getLogger } from "@logtape/logtape";
import { getElementById } from "domutils";
import { ElementType, parseDocument } from "htmlparser2";
import { dirname, join } from "path";
import * as v from "valibot";
import {
	commands,
	DocumentFilter,
	EventEmitter,
	ExtensionContext,
	languages,
	Selection,
	TextEditorRevealType,
	Uri,
	window,
	workspace,
} from "vscode";
import { CancellationToken } from "vscode-languageclient";
import { Settings } from "../settings";
import { ManifestSettings } from "../workspaces/manifest";
import { OGDefinitionProvider } from "./definitions";
import { OGTextDocumentContentProvider } from "./documents";
import { OGReferenceProvider } from "./references";
import { OGDefinitions, OGSearchQuery, OGSearchResults } from "./schemas";
import { OGDocumentSymbolProvider } from "./symbols";

export * from "./schemas";

export class OpenGrokApi {
	readonly #ctx: ExtensionContext;
	readonly #s: Settings;
	readonly #manifest: ManifestSettings;
	readonly #logger = getLogger(["CAS", "OG", "api"]);

	get url() {
		try {
			return new URL(
				this.#manifest.opengrok?.url ??
					this.#s.OGUrl ??
					"error://not-configured",
			);
		} catch (_e) {
			return new URL("error://not-configured");
		}
	}
	#urlChangeEventEmitter = new EventEmitter<URL>();
	public urlChangeEvent = this.#urlChangeEventEmitter.event;

	get apiKey() {
		return this.url.protocol !== "error:"
			? (this.#manifest.opengrok?.apiKey ??
					this.#ctx.secrets
						.get(`OG.API_KEY.${this.#s.OGUrl}`)
						.then((v) => v ?? this.#s.OGUrl))
			: undefined;
	}
	constructor(
		context: ExtensionContext,
		settings: Settings,
		manifestSettings: ManifestSettings,
	) {
		this.#ctx = context;
		this.#s = settings;
		this.#manifest = manifestSettings;

		const docFilters: DocumentFilter[] = [
			{
				scheme: "opengrok",
			},
		];

		if (settings.depsFolders.length) {
			docFilters.push(
				...settings.depsFolders.map((depsFolder) => ({
					pattern: join(depsFolder, "/**/*"),
				})),
			);
		}

		this.#ctx.subscriptions.push(
			workspace.registerTextDocumentContentProvider(
				"opengrok",
				new OGTextDocumentContentProvider(this),
			),
			languages.registerDefinitionProvider(
				docFilters,
				new OGDefinitionProvider(this),
			),
			languages.registerReferenceProvider(
				docFilters,
				new OGReferenceProvider(this),
			),
			languages.registerDocumentSymbolProvider(
				docFilters,
				new OGDocumentSymbolProvider(this),
				{
					label: "OpenGrok",
				},
			),
			commands.registerCommand("cas.og.apiKey.set", async () => {
				const apiKey = await window.showInputBox({
					title: "OpenGrok API Key",
					prompt: `API Key for ${this.#s.OGUrl}`,
					ignoreFocusOut: true,
					password: true,
				});
				if (apiKey !== undefined) {
					this.#logger.debug`Changed API Key for ${this.#s.OGUrl}`;
					await this.setApiKey(apiKey);
				}
			}),
			commands.registerCommand("cas.setOpenGrokURL", async (url?: string) => {
				if (url === undefined) {
					url = await window.showInputBox({
						title: "OpenGrok URL",
						placeHolder: "https://opengrok.example.com",
					});
				}
				await this.#s.set("openGrokUrl", url);
				this.#urlChangeEventEmitter.fire(this.url);
			}),
		);
	}

	async setApiKey(apiKey: string) {
		return this.#ctx.secrets.store(`OG.API_KEY.${this.#s.OGUrl}`, apiKey);
	}

	public async search(
		query: OGSearchQuery | string,
		signal?: AbortSignal,
	): Promise<OGSearchResults> {
		if (typeof query === "string") {
			query = {
				full: query,
			};
		}
		query.type = query.type?.toLowerCase() === "any" ? undefined : query.type;
		const queryUrl = new URL(
			join(this.url.pathname, "api/v1/search"),
			this.url,
		);
		const search = new URLSearchParams();
		for (const [key, value] of Object.entries(query)) {
			if (Array.isArray(value)) {
				for (const v of value) {
					search.append(key, v);
				}
			} else if (value !== undefined) {
				search.append(key, value?.toString());
			}
		}
		queryUrl.search = search.toString();
		const results = await http.get(queryUrl, {
			signal,
		});
		if (!results.ok) {
			this.#logger.warn`Search failed: ${results.status} ${results.statusText}`;
			return {
				time: 0,
				resultCount: 0,
				startDocument: -1,
				endDocument: -1,
				results: {},
			};
		}
		try {
			const data = await results.json();
			const ogResults = v.parse(OGSearchResults, data);
			this.#logger
				.debug`Found ${ogResults.resultCount} results in ${ogResults.time}ms`;
			return ogResults;
		} catch (e) {
			this.#logger.error`Failed parsing results: ${e}`;
			return {
				time: 0,
				resultCount: 0,
				startDocument: -1,
				endDocument: -1,
				results: {},
			};
		}
	}
	public async getFile(path: string, signal?: AbortSignal) {
		const fileUrl = new URL(
			join(
				this.url.pathname,
				`api/v1/file/content?path=${encodeURIComponent(path)}`,
			),
			this.url,
		);
		const response = await http.get(fileUrl, {
			signal,
			headers: {
				Authorization: `Bearer ${await this.apiKey}`,
			},
		});
		if (response.status === 401) {
			const fileUrl = new URL(join(this.url.pathname, "raw", path), this.url);
			const response = await http.get(fileUrl, {
				signal,
			});
			return response.text();
		}
		return response.text();
	}
	public async getProjects(signal?: AbortSignal): Promise<string[]> {
		const response = await http.get(
			new URL(join(this.url.pathname, "api/v1/projects"), this.url),
			{
				signal,
				headers: {
					Authorization: `Bearer ${await this.apiKey}`,
				},
			},
		);
		if (response.ok) {
			return response.json<string[]>();
		} else if (response.status === 401) {
			const response = await http.get(this.url, {
				signal,
			});
			const dom = parseDocument(await response.text());
			const projectSelect = getElementById("project", dom.children, true);
			const projects = new Set<string>();
			for (const option of projectSelect?.children ?? []) {
				if (
					option.type === ElementType.Tag &&
					option.tagName === "option" &&
					option.attribs.value
				) {
					projects.add(option.attribs.value);
				}
			}
			return [...projects];
		}
		return [];
	}

	public async getDefinitions(
		file: Uri,
		signal?: AbortSignal,
	): Promise<OGDefinitions> {
		const response = await http.get(
			new URL(
				join(
					this.url.pathname,
					`api/v1/file/defs?path=${encodeURIComponent(file.path)}`,
				),
				this.url,
			),
			{
				signal,
				headers: {
					Authorization: `Bearer ${await this.apiKey}`,
				},
			},
		);
		if (response.ok) {
			return v.parse(OGDefinitions, await response.json());
		}
		return [];
	}

	public async openFileInEditor(uri: Uri, line?: number) {
		const matchingFiles = await workspace.findFiles(
			uri.fsPath.slice(uri.fsPath.indexOf("/", 1) + 1),
		);
		if (matchingFiles.length === 1) {
			await window.showTextDocument(matchingFiles[0]);
		} else {
			await window.showTextDocument(uri);
		}
		const editor = window.activeTextEditor;
		if (line !== undefined) {
			// lines are 0-indexed
			let range = editor?.document.lineAt(line - 1).range;

			if (range && editor) {
				// move cursor to start of the line
				editor.selection = new Selection(range.start, range.start);
				// center the line
				editor.revealRange(range, TextEditorRevealType.InCenter);
			}
		}
	}

	public toProjectUri(uri: Uri, project: string): Uri {
		return uri.with({
			path: withLeadingSlash(
				join(
					project,
					this.#s.depsFolders.reduce(
						(path, folder) => path.replace(dirname(folder), ""),
						uri.path,
					),
				),
			),
		});
	}

	public async toDocumentUri(
		path: string,
		projects: string[],
		token?: CancellationToken,
	) {
		const localPath = projects.reduce(
			(path, project) =>
				path.startsWith(project) ? path.slice(project.length) : path,
			withoutLeadingSlash(path),
		);
		const localFile = (
			await workspace.findFiles(
				withoutLeadingSlash(localPath),
				undefined,
				undefined,
				token,
			)
		).at(0);
		return localFile || Uri.parse(`opengrok://${path}#${projects.join(",")}`);
	}
}
