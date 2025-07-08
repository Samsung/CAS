<script module lang="ts">
export interface OGSearchQuery {
	symbol?: string | undefined;
	type?: (typeof types)[number] | undefined;
	full?: string | undefined;
	def?: string | undefined;
	path?: string | undefined;
	hist?: string | undefined;
	projects?: string[] | undefined;
	maxresults?: number;
	start?: number;
}
export const textQueryFields: (keyof OGSearchQuery)[] = [
	"symbol",
	"def",
	"full",
] as const;
</script>
<script lang="ts">
    import { types } from "./fileTypes.json";
    import type { MaybeAsync } from "$lib/types.d.ts";
    import type { HTMLAttributes } from 'svelte/elements';
    import VsCodeTooltipButton from "$lib/vscode/VSCodeTooltipButton.svelte";
    import { stopPropagation } from "$lib/helpers";
    import * as t from "$lib/paraglide/messages.js";
    import type { OGSearchResults } from "./types";
    import type { InputEventFor } from "$lib/types";
      import type { VscodeMultiSelect, VscodeSingleSelect, VscodeTextfield } from "@vscode-elements/elements";
    interface Props extends HTMLAttributes<HTMLElement> {
        search: (query: OGSearchQuery, signal: AbortSignal) => MaybeAsync<OGSearchResults>;
        query?: OGSearchQuery;
        searchResults?: OGSearchResults["results"];
        projects?: string[];
        debounceTime?: number;
        enabled?: boolean;
    }

    let {
        search,
        query=$bindable({
        projects: [],
        type: types[0],
        maxresults: 25
        }),
        projects = [],
        debounceTime = 500,
        enabled = true,
        ...props
    }: Props = $props();

    let waiting = $state(false);
    let searchBox: VscodeTextfield;

    let abort = new AbortController()
    async function runSearch() {
        waiting = true;
        abort.abort("started new search");
        abort = new AbortController();
        const queryToUse = {
            full: searchBox.value,
            ...$state.snapshot(usedQuery),
        };
        query = queryToUse;
        await search(queryToUse, abort.signal)
        waiting = false;
    }
    let advancedOpen = $state(false);
    let usedQuery: OGSearchQuery = $state({
        projects: query.projects ?? [],
        type: query.type ?? types[0],
        maxresults: query.maxresults ?? 25
    });
    let selectedProjects = $state(["android"]);
    $effect(() => {
        usedQuery.projects = [...$state.snapshot(selectedProjects)];
    })
    
</script>
    
    
<search 
    class="sticky top-0 z-10 flex flex-row -ml-[22px] bg-vscode-panel-background"
    {...props}
>
    <VsCodeTooltipButton
        onclick={stopPropagation(() => advancedOpen = !advancedOpen)}
        description="Advanced"
        icon={advancedOpen ? "chevron-down" : "chevron-right"}
    >
    </VsCodeTooltipButton>
    <fieldset class="flex flex-col grow">
        <vscode-textfield 
            value={query.full}
            bind:this={searchBox}
            onkeyup={(e: KeyboardEvent) => {if (e.key === "Enter") runSearch()}}
            class="grow w-full"
            placeholder={t.search()}
            disabled={!enabled || undefined}
        >
        <span slot="content-before" class="flex flex-row h-auto py-0 mt-0">
            <VsCodeTooltipButton
            description="Search"
            placement="bottom"
            onclick={runSearch}
            vsbutton={false}
            class="flex items-baseline"
            >
                {#if waiting}
                    <vscode-progress-ring class="h-4 w-4"></vscode-progress-ring>
                {:else}
                    <vscode-icon name="search" size="16"></vscode-icon>
                {/if}
            </VsCodeTooltipButton>
        </span>
        <span slot="content-after" class="flex flex-row justify-end h-auto py-0 -mt-0.5">
            
        </span>
    </vscode-textfield>
    <fieldset class:hidden={!advancedOpen} class="flex flex-col gap-2 mt-2">
        <div class="flex flex-col flex-nowrap align-middle justify-between">
            <label for="type" class="block text-vscode-foreground mb-0.5">Projects:</label>
            <vscode-multi-select value={selectedProjects} onchange={(e: InputEvent) => selectedProjects = (e.currentTarget as VscodeMultiSelect).value}>
                {#each projects as project}
                    <vscode-option value={project}>{project}</vscode-option>
                {/each}
            </vscode-multi-select>
        </div>
        <div class="flex flex-row flex-nowrap align-middle justify-between">
            <label for="type" class="block text-vscode-foreground mb-0.5">Definition:</label>
            <vscode-textfield value={query.def ?? ""} onchange={(e: InputEvent) => usedQuery.def = (e.currentTarget as HTMLInputElement).value} ></vscode-textfield>
        </div>
        <div class="flex flex-row flex-nowrap align-middle justify-between">
            <label for="type" class="block text-vscode-foreground mb-0.5">Symbol:</label>
            <vscode-textfield value={query.symbol ?? ""} onchange={(e: InputEvent) => usedQuery.symbol = (e.currentTarget as HTMLInputElement).value} ></vscode-textfield>
        </div>
        <div class="flex flex-row flex-nowrap align-middle justify-between">
            <label for="type" class="block text-vscode-foreground mb-0.5">File path:</label>
            <vscode-textfield value={query.path ?? ""} onchange={(e: InputEvent) => usedQuery.path = (e.currentTarget as HTMLInputElement).value} ></vscode-textfield>
        </div>
        <div class="flex flex-row flex-nowrap align-middle justify-between">
            <label for="type" class="block text-vscode-foreground mb-0.5">Type:</label>
            <vscode-single-select id="type" value={usedQuery.type ?? "Any"} onchange={(e: InputEventFor<VscodeMultiSelect>) => usedQuery.type = e.currentTarget.value.join(",") }>
                {#each types as type}
                 <vscode-option value={type}>{type}</vscode-option>
                {/each}
            </vscode-single-select>
        </div>
        <div class="flex flex-row flex-nowrap align-middle justify-between">
            <label for="maxresults" class="block text-vscode-foreground mb-0.5">Number of results to return:</label>
            <vscode-single-select id="type" value={usedQuery.maxresults} onchange={(e: InputEventFor<VscodeSingleSelect>) => usedQuery.maxresults = parseInt(e.currentTarget.value) }>
                {#each [10, 25, 50, 75, 100] as count}
                 <vscode-option value={count}>{count}</vscode-option>
                {/each}
            </vscode-single-select>
        </div>
    </fieldset>
    </fieldset>
    
</search>