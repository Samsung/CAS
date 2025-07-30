<script lang="ts">
import uFuzzy from "@leeoniya/ufuzzy";
import { copy } from "@svelte-put/copy";
import type { Snippet } from "svelte";
import { fade } from "svelte/transition";
import type { MaybeAsync } from "$lib/types.d.ts";
import VsCodePaginator from "./VSCodePaginator.svelte";

interface Props {
	perPage?: number;
	page?: number;
	pages?: number;
	itemCount?: number;
	rowsData: Record<string, string>[];
	columns?: string[];
	"generate-header"?: "default" | "sticky" | "none";
	renderItem?: Snippet<[string, string, Record<string, string>]>;
	searchable?: boolean;
	onPageChange?: (e: UIEvent) => MaybeAsync<unknown>;
	onSearch?: (
		searchTerm: string,
		page: number,
		signal?: AbortSignal,
	) => MaybeAsync<unknown>;
}
let {
	perPage = 20,
	page = $bindable(1),
	pages: maxPages,
	rowsData,
	columns: templateColumns,
	"generate-header": header = "default",
	renderItem,
	searchable = false,
	onPageChange,
	onSearch,
	itemCount,
}: Props = $props();

const uf = new uFuzzy({
	unicode: true,
});
let searchTerm = $state("");
const filtered = $derived(
	searchable && rowsData && searchTerm.length
		? (uf
				.search(
					rowsData.map((data) => Object.values(data).join(" ")),
					searchTerm,
				)[0]
				?.map((id) => rowsData[id] ?? {}) ?? rowsData)
		: rowsData,
);
const paginated = $derived(
	filtered.slice((page - 1) * perPage, page * perPage),
);
// svelte-ignore state_referenced_locally handled below
let prevLen = $state(paginated.length);
$effect(() => {
	if (paginated.length > 0 && Math.abs(paginated.length - prevLen) > 2) {
		prevLen = paginated.length;
	}
});
let abortController = new AbortController();

let expanded = $state<Record<number, boolean>>({});
</script>

{#snippet paginator()}
<div class="flex flex-row justify-between">
    {#if rowsData.length > perPage}
        <VsCodePaginator
            bind:page={page}
            count={itemCount && searchTerm === "" ? itemCount : filtered.length}
            {perPage}
            showRange={true}
            ellipsis={true}
            onChange={(e) => {if (onPageChange && !searchTerm.length) onPageChange(e)}}
        ></VsCodePaginator>
        {#if searchable}
            <vscode-textfield
                placeholder="Search"
                value={searchTerm}
                oninput={(e: InputEvent) => {
                    searchTerm = (e.target as HTMLInputElement).value;
                    if(onSearch) {
                        abortController.abort();
                        abortController = new AbortController();
                        onSearch(searchTerm, page, abortController.signal)
                    }
                }}
                class="justify-self-end"
            ></vscode-textfield>
        {/if}
    {/if}
</div>
{/snippet}
<div>
    {@render paginator()}
    <vscode-table columns={templateColumns}>
        {#if header !== "none"}
            <vscode-table-header slot="header" class="grid" style={`grid-template-columns: ${templateColumns?.join(" ") ?? `repeat(1fr, ${Object.keys(rowsData?.at(0) ?? {}).length})`}`}>
                {#each Object.keys(rowsData?.at(0) ?? {}) as key, i}
                    <vscode-table-header-cell>{key}</vscode-table-header-cell>
                {/each}
            </vscode-table-header>
        {/if}
        <vscode-table-body slot="body">
            {#each paginated as item, i}
                {#if item}
                <vscode-table-row transition:fade={{delay: 75, duration: 400}} class="grid" style={`grid-template-columns: ${templateColumns?.join(" ") ?? `repeat(1fr, ${Object.keys(item).length})`}`}>
                    {#each Object.values(item) as value, i}
                    <vscode-table-cell  use:copy={{text: value}} class="h-auto" class:text-pretty={expanded[i]} onclick={() => window.getSelection()?.type !== "Range" ? expanded[i] = !expanded[i] : undefined}>
                        {#if renderItem}
                        {@render renderItem(value, Object.keys(item)[i], item)}
                        {:else}
                        {value}
                        {/if}
                    </vscode-table-cell>
                    {/each}
                </vscode-table-row>
                {:else}
                <!-- prevent UI from jumping around by rendering empty rows while waiting for data -->
                <vscode-table-row>
                    {#each Object.keys(rowsData?.at(0) ?? {e:0}) as _, i}
                    <vscode-table-cell ></vscode-table-cell>
                    {/each}
                </vscode-table-row> 
                {/if}
            {/each}
            {#if !paginated.length && rowsData.length}
                <!-- prevent UI from jumping around by rendering empty rows while waiting for data -->
                {#each new Array(prevLen) as _}
                    <vscode-table-row class="h-6">
                        {#each Object.keys(rowsData?.at(0) ?? {}) as _, i}
                        <vscode-table-cell >
                        </vscode-table-cell>
                        {/each}
                    </vscode-table-row> 
                {/each}
            {/if}
        </vscode-table-body>
    </vscode-table>
    {@render paginator()}
</div>