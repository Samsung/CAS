<script lang="ts">
import { browser } from "$app/environment";
import TooltipText from "$lib/base/TooltipText.svelte";
import { decodeHtmlCharCodes } from "$lib/helpers";
import IntersectionObserver from "$lib/IntersectionObserver.svelte";
import { loadHtml } from "$lib/og/loadHtml";
import OgSearch, {
	type OGSearchQuery,
	textQueryFields,
} from "$lib/og/OgSearch.svelte";
import type { OGSearchResult, OGSearchResults } from "$lib/og/types";
import * as t from "$lib/paraglide/messages.js";
import { type CasApiEvent, vscode } from "$lib/vscode";
import { observeThemeId } from "$lib/vscode/themeObserver.svelte";
import VsCodePaginator from "$lib/vscode/VSCodePaginator.svelte";
import { debounce } from "@melt-ui/svelte/internal/helpers";
import QuickLRU from "quick-lru";
import sanitize from "sanitize-html";
import { onMount, tick } from "svelte";
import { SvelteSet } from "svelte/reactivity";

let projects: string[] = $state([]);
let enabled = $state(true);
let query: OGSearchQuery = $state({});
type InitEvent = CasApiEvent<{ projects: string[]; enabled: boolean }>;
type SearchEvent = CasApiEvent<OGSearchResults>;
const themeObserver = observeThemeId();
let theme = $derived(themeObserver.theme);
onMount(async () => {
	({
		data: { projects, enabled },
	} = (await vscode.execFunction("init", {})) as InitEvent);
	enabled = true;
	vscode.addEventListener("init", (event) => {
		if (
			(event.data as CasApiEvent)?.data &&
			typeof (event.data as CasApiEvent)?.data === "object" &&
			"enabled" in (event.data as CasApiEvent<{}>)?.data
		) {
			enabled = !!(event.data as CasApiEvent<{ enabled: boolean }>).data
				.enabled;
		}
	});
});
let waiting = $state(false);
async function search(query: OGSearchQuery, signal?: AbortSignal) {
	waiting = true;
	pages = pages ?? 1;
	const { data } = (await vscode.execFunction("search", {
		data: query,
	})) as SearchEvent;
	results = data;
	pages = Math.ceil(data.resultCount / (query.maxresults ?? 25));
	waiting = false;
	let lastPromise = Promise.resolve();
	for (const [path, result] of Object.entries(data?.results ?? {})) {
		lastPromise = lastPromise.then(async () => {
			await tick();
			await loadFile(
				path,
				result.map((v) =>
					parseInt(
						typeof v.lineNumber === "number" || v.lineNumber.length
							? v.lineNumber
							: "1",
					),
				),
				result.map((v) => v.line),
			);
		});
	}
	tick().then(() => vscode.setStateKeys({ results: data, query }));
	return data;
}
let results: OGSearchResults = $state({
	time: 0,
	startDocument: 0,
	endDocument: 0,
	resultCount: 0,
	results: {},
});

async function getFile(path: string) {
	const { data } = (await vscode.execFunction("file", {
		data: path,
	})) as CasApiEvent<string>;
	return data;
}

const htmlCache = new QuickLRU<string, { usedTheme: string; html: string }>({
	maxSize: 50,
	onEviction(key, value) {
		vscode.setStateKey("og-html-cache", [...htmlCache.entries()].slice(0, 3));
	},
});

const loadedFiles = $state(new SvelteSet<string>());

async function loadFile(path: string, lines: number[], knownLines: string[]) {
	if (htmlCache.has(path)) {
		return htmlCache.get(path)?.html;
	}
	const file = await getFile(path);
	if (
		// do not highlight huge files
		file.length > 512_000
	) {
		throw new Error();
	}
	const fullQuery = Object.entries(query)
		.filter(
			([k, v]) =>
				typeof v === "string" &&
				textQueryFields.includes(k as keyof OGSearchQuery),
		)
		.map(([_, v]) => v)
		.join(" ");
	const html = await loadHtml(path, file, lines, theme, fullQuery);

	htmlCache.set(path, { html, usedTheme: theme });
	tick().then(() => {
		loadedFiles.add(path);
		vscode.setStateKey("og-html-cache", [...htmlCache.entries()].slice(0, 3));
	});
	return html;
}
async function highlightFile(
	path: string,
	result: OGSearchResult[],
	knownLines: string[],
	retried = false,
) {
	const lines = result.map((v) => parseInt(v.lineNumber));
	let { html, usedTheme } = htmlCache.get(path) ?? {
		html: undefined,
		usedTheme: undefined,
	};
	if (usedTheme === theme) {
		return html;
	}

	if (!retried) {
		await tick();
		return new Promise((resolve) =>
			setTimeout(
				async () =>
					resolve(await highlightFile(path, result, knownLines, true)),
				250,
			),
		);
	}
	return loadFile(path, lines, knownLines);
}
let codeList: Record<string, HTMLDivElement> = $state({});
function createGoToLine(path: string) {
	return function goToLine(e: MouseEvent) {
		let target = e.target as HTMLElement | null;
		while (!target?.classList.contains("line") && target !== e.currentTarget) {
			target = target?.parentElement ?? (e.currentTarget as HTMLElement);
		}
		if (!target) {
			return;
		}
		const line = parseInt(target.getAttribute("data-line") ?? "-1");
		vscode.postMessage({
			func: "openLine",
			data: {
				path,
				line,
				projects: $state.snapshot(query.projects),
			},
		});
	};
}
$effect(() => {
	for (const [path, div] of Object.entries(codeList)) {
		if (div && !div.getAttribute("listener")) {
			div.addEventListener("click", createGoToLine(path));
			div.setAttribute("listener", "added");
		}
	}
});
let observedElements: Record<string, HTMLElement> = $state({});
let page = $state(1);
let pages = $state(1);

if (browser) {
	const {
		results: oldResults,
		query: oldQuery,
		"og-html-cache": ogHtmlCache,
	} = (vscode.getState() ?? {
		results: undefined,
		query: {
			full: "",
			def: "",
			symbol: "",
			path: "",
		},
		"og-html-cache": undefined,
	}) as {
		results?: OGSearchResults;
		query?: OGSearchQuery;
		"og-html-cache"?: [string, { usedTheme: string; html: string }][];
	};
	if (oldResults) {
		results = oldResults;
		pages = Math.ceil(
			oldResults.resultCount /
				(oldQuery?.maxresults ??
					oldResults.endDocument - oldResults.startDocument),
		);
	}
	if (oldQuery) {
		query = oldQuery;
	}
	(async () => {
		for (const [key, value] of ogHtmlCache ?? []) {
			htmlCache.set(key, value);
			await tick();
		}
	})();
}
</script>

<OgSearch {search} {projects} bind:query={query} {enabled}></OgSearch>
{#snippet paginator(pages: number)}
<VsCodePaginator 
    bind:page={page}
    count={pages}
    ellipsis={true}
    siblingCount={1}
    onChange={debounce((e => {
        query.start = (query?.maxresults ?? 10) * (e.detail - 1);
        return search($state.snapshot(query))
    }), 10)}
/>
{/snippet}
{#if results && results.resultCount}
    {@render paginator(pages)}
    {#if !waiting && enabled}
        <output>
            {#each Object.entries(results.results) as [path, value], i (path)}
                    <div
                        data-vscode-context={JSON.stringify({
                            elem_type: "path",
                            v: path,
                        })}
                        bind:this={observedElements[path]}
                    >
                        <vscode-collapsible title={path} open>
							<vscode-badge variant="counter" slot="decorations">{value.length}</vscode-badge>
                        	{#key theme + loadedFiles.has(path)}
                                {#snippet simpleListing()}
                                    <vscode-table>
										<vscode-table-body slot="body">

											{#each value as item}
												<vscode-table-row>
													<vscode-table-cell
													onclick={createGoToLine(path)}
													class="w-full justify-start" 
													
													data-line={item.lineNumber}
													>{@html sanitize(
														item.line,
													).replaceAll("<b>", '<b class="highlighted-word">')}</vscode-table-cell
													>
												</vscode-table-row>
											{/each}
										</vscode-table-body> 
                                    </vscode-table>
                                {/snippet}
                                {#await highlightFile(path, value, value.map(item => item.line))}
                                    {@render simpleListing()}
                                {:then highlighted}
                                    <div
                                        bind:this={codeList[path]}
                                        data-vscode-context={JSON.stringify({
                                            elem_type: "path",
                                            v: path,
                                        })}
                                    >
                                        {@html highlighted}
                                    </div>
                                {:catch}
                                    {@render simpleListing()}
                                {/await}
                            {/key}
						</vscode-collapsible>
                    </div>
            {/each}
        </output>
	
    {:else}
        <vscode-progress-ring></vscode-progress-ring>
    {/if}
    {@render paginator(pages)}
{:else if !enabled}
<div class="flex flex-col w-full items-center">
	<h1 class="block text-vscode-editorError-foreground w-max text-2xl">OpenGrok not configured</h1>
	<p class="block w-max"><a href="command:cas.setOpenGrokURL" class="text-vscode-textLink-activeForeground">Configure the URL in settings</a> or use a workspace file</p>
</div>
{/if}

<style lang="postcss">
    :global(.tab::before) {
        content: "⇥";
        position: absolute;
        opacity: 0.2;
    }

    :global(.space::before) {
        content: "·";
        position: absolute;
        opacity: 0.1;
    }
    :global(.highlighted-word) {
        background-color: var(--vscode-editor-findMatchHighlightBackground);
        border: 1px solid var(--vscode-editor-findMatchHighlightBorder);
    }
    :global(code) {
        white-space: pre-wrap;
        word-wrap: break-word;
    }
    :global(.line) {
        padding-left: 2.5rem;
    }
    :global(code .line::before) {
        display: inline-block;
        content: attr(data-line);
        width: 2rem;
        margin-right: 0.5rem;
        text-align: right;
        font-variant-numeric: tabular-nums;
        color: var(--colour-line-numbers);
        margin-left: -2.5rem;
    }
    :global(.hiddenButAccessible) {
        position: absolute;
        left: -9999px;
        max-height: 0px;
        overflow: hidden;
        opacity: 0;
    }
    :global(.line:has(+ .hiddenButAccessible)::after) {
        display: inline-block;
        content: "";
        border-top: 0.5rem solid var(--vscode-panel-background);
        width: 150vw;
        margin-left: -50vw;
        transform: translateY(0.6rem);
    }
    :global(
            .code-group-1:hover,
            .code-group-1:hover ~ .code-group-1,
            .code-group-1:has(~ .code-group-1:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

	:global(
			.code-group-2:hover,
			.code-group-2:hover ~ .code-group-2,
			.code-group-2:has(~ .code-group-2:hover)
	) {
		background-color: var(--vscode-list-hoverBackground);
	}

	:global(
			.code-group-3:hover,
			.code-group-3:hover ~ .code-group-3,
			.code-group-3:has(~ .code-group-3:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
	}

    :global(
            .code-group-4:hover,
            .code-group-4:hover ~ .code-group-4,
            .code-group-4:has(~ .code-group-4:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
	}

    :global(
            .code-group-5:hover,
            .code-group-5:hover ~ .code-group-5,
            .code-group-5:has(~ .code-group-5:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-6:hover,
            .code-group-6:hover ~ .code-group-6,
            .code-group-6:has(~ .code-group-6:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-7:hover,
            .code-group-7:hover ~ .code-group-7,
            .code-group-7:has(~ .code-group-7:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-8:hover,
            .code-group-8:hover ~ .code-group-8,
            .code-group-8:has(~ .code-group-8:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-9:hover,
            .code-group-9:hover ~ .code-group-9,
            .code-group-9:has(~ .code-group-9:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-10:hover,
            .code-group-10:hover ~ .code-group-10,
            .code-group-10:has(~ .code-group-10:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-11:hover,
            .code-group-11:hover ~ .code-group-11,
            .code-group-11:has(~ .code-group-11:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-12:hover,
            .code-group-12:hover ~ .code-group-12,
            .code-group-12:has(~ .code-group-12:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-13:hover,
            .code-group-13:hover ~ .code-group-13,
            .code-group-13:has(~ .code-group-13:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-14:hover,
            .code-group-14:hover ~ .code-group-14,
            .code-group-14:has(~ .code-group-14:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-15:hover,
            .code-group-15:hover ~ .code-group-15,
            .code-group-15:has(~ .code-group-15:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-16:hover,
            .code-group-16:hover ~ .code-group-16,
            .code-group-16:has(~ .code-group-16:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-17:hover,
            .code-group-17:hover ~ .code-group-17,
            .code-group-17:has(~ .code-group-17:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-18:hover,
            .code-group-18:hover ~ .code-group-18,
            .code-group-18:has(~ .code-group-18:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-19:hover,
            .code-group-19:hover ~ .code-group-19,
            .code-group-19:has(~ .code-group-19:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-20:hover,
            .code-group-20:hover ~ .code-group-20,
            .code-group-20:has(~ .code-group-20:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-21:hover,
            .code-group-21:hover ~ .code-group-21,
            .code-group-21:has(~ .code-group-21:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-22:hover,
            .code-group-22:hover ~ .code-group-22,
            .code-group-22:has(~ .code-group-22:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-23:hover,
            .code-group-23:hover ~ .code-group-23,
            .code-group-23:has(~ .code-group-23:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-24:hover,
            .code-group-24:hover ~ .code-group-24,
            .code-group-24:has(~ .code-group-24:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-25:hover,
            .code-group-25:hover ~ .code-group-25,
            .code-group-25:has(~ .code-group-25:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-26:hover,
            .code-group-26:hover ~ .code-group-26,
            .code-group-26:has(~ .code-group-26:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-27:hover,
            .code-group-27:hover ~ .code-group-27,
            .code-group-27:has(~ .code-group-27:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-28:hover,
            .code-group-28:hover ~ .code-group-28,
            .code-group-28:has(~ .code-group-28:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-29:hover,
            .code-group-29:hover ~ .code-group-29,
            .code-group-29:has(~ .code-group-29:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-30:hover,
            .code-group-30:hover ~ .code-group-30,
            .code-group-30:has(~ .code-group-30:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-31:hover,
            .code-group-31:hover ~ .code-group-31,
            .code-group-31:has(~ .code-group-31:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-32:hover,
            .code-group-32:hover ~ .code-group-32,
            .code-group-32:has(~ .code-group-32:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-33:hover,
            .code-group-33:hover ~ .code-group-33,
            .code-group-33:has(~ .code-group-33:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-34:hover,
            .code-group-34:hover ~ .code-group-34,
            .code-group-34:has(~ .code-group-34:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-35:hover,
            .code-group-35:hover ~ .code-group-35,
            .code-group-35:has(~ .code-group-35:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-36:hover,
            .code-group-36:hover ~ .code-group-36,
            .code-group-36:has(~ .code-group-36:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-37:hover,
            .code-group-37:hover ~ .code-group-37,
            .code-group-37:has(~ .code-group-37:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-38:hover,
            .code-group-38:hover ~ .code-group-38,
            .code-group-38:has(~ .code-group-38:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-39:hover,
            .code-group-39:hover ~ .code-group-39,
            .code-group-39:has(~ .code-group-39:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-40:hover,
            .code-group-40:hover ~ .code-group-40,
            .code-group-40:has(~ .code-group-40:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-41:hover,
            .code-group-41:hover ~ .code-group-41,
            .code-group-41:has(~ .code-group-41:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-42:hover,
            .code-group-42:hover ~ .code-group-42,
            .code-group-42:has(~ .code-group-42:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-43:hover,
            .code-group-43:hover ~ .code-group-43,
            .code-group-43:has(~ .code-group-43:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-44:hover,
            .code-group-44:hover ~ .code-group-44,
            .code-group-44:has(~ .code-group-44:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-45:hover,
            .code-group-45:hover ~ .code-group-45,
            .code-group-45:has(~ .code-group-45:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-46:hover,
            .code-group-46:hover ~ .code-group-46,
            .code-group-46:has(~ .code-group-46:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-47:hover,
            .code-group-47:hover ~ .code-group-47,
            .code-group-47:has(~ .code-group-47:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-48:hover,
            .code-group-48:hover ~ .code-group-48,
            .code-group-48:has(~ .code-group-48:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-49:hover,
            .code-group-49:hover ~ .code-group-49,
            .code-group-49:has(~ .code-group-49:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }

    :global(
            .code-group-50:hover,
            .code-group-50:hover ~ .code-group-50,
            .code-group-50:has(~ .code-group-50:hover)
 	) {
		background-color: var(--vscode-list-hoverBackground);
        }
</style>
