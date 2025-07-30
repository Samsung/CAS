<script lang="ts">
import { createPagination, melt } from "@melt-ui/svelte";
import type { ChangeFn } from "@melt-ui/svelte/internal/helpers";
import type { VscodeTextfield } from "@vscode-elements/elements";
import type { InputEventFor } from "$lib/types";

interface Props {
	onChange?: (event: UIEvent) => unknown;
	page: number;
	count: number;
	perPage?: number;
	ellipsis?: boolean;
	siblingCount?: number;
	showRange?: boolean;
}
let {
	onChange,
	page: currentPage = $bindable(1),
	count = $bindable(1),
	perPage = 1,
	ellipsis = true,
	siblingCount = 2,
	showRange = false,
}: Props = $props();
const goToPage: ChangeFn<number> = ({ curr, next }) => {
	if (next > count || next < 1 || isNaN(next) || curr === next) {
		return curr;
	}
	currentPage = next;
	if (onChange) {
		onChange(
			new UIEvent("change", {
				cancelable: false,
				detail: next,
				bubbles: true,
			}),
		);
	}
	return next;
};
const {
	options: {
		count: countStore,
		siblingCount: siblingCountStore,
		perPage: perPageStore,
	},
	elements: { root, pageTrigger, prevButton, nextButton },
	states: { pages, range, page: meltPage },
} = createPagination({
	count: count,
	perPage,
	defaultPage: currentPage,
	siblingCount,
	onPageChange: goToPage,
});
$effect(() => perPageStore.set(perPage));
$effect(() => countStore.set(count));
$effect(() => meltPage.set(currentPage));
const pageWidth = $derived(Math.ceil(Math.log10(count) * 0.4 + 0.01));

// this mess is the result of wanting constant number (sum) of visible buttons and ellipses
// while at the same time converting a button to ellipse only when we've "left behind" its side, with first/last page to each side.
// So e.g. with siblingSize=2 and 11 pages, we'd want to show 1 [2] 3 4 5 6 7 [8] 11 when 5 is selected, but 1 2 3 4 5 6 7 [8] 11
// and then 1 [4] 5 6 7 8 9 10 11 when >=8 is selected.
// turns out it's harder than I thought... Though I assume there must be a more elegant way than *this*,
// so if you find it, feel free to fix this mess
const pageButtons = $derived(
	count / perPage > siblingCount * 2 + 3 &&
		count > siblingCount * 2 + 3 &&
		(currentPage <= siblingCount * 2 ||
			currentPage > count / perPage - siblingCount * 2 + 1)
		? $pages.toSpliced(
				currentPage <= siblingCount * 2 ? -2 : 2,
				0,
				{
					type: "page",
					value: siblingCount * 2 + 1 + +(currentPage <= siblingCount * 2),
					key: "ellipsis-jump-fix",
				},
				{
					type: "page",
					value: siblingCount * 2 + 2 + +(currentPage <= siblingCount * 2),
					key: "ellipsis-jump-fix-2",
				},
			)
		: $pages,
);
</script>

<nav class="flex flex-row gap-1 justify-evenly items-center max-w-full" style={`--pageCount:${count};--pageWidth:${pageWidth};`} use:melt={$root}>
    <button use:melt={$prevButton} aria-label="previous page">
		{#if $prevButton.disabled}
			<vscode-icon action-icon name="chevron-left" disabled></vscode-icon>
		{:else}
			<vscode-icon action-icon name="chevron-left"></vscode-icon>
		{/if}
	</button>
    {#each pageButtons as page, i (page.key)}
		{#if page.type === "ellipsis"}
			<vscode-textfield
				size={pageWidth}
				placeholder={i<pageButtons.length/2 ? (pageButtons.at(2) as {value: number})?.value-1 : (pageButtons.at(-3) as {value: number})?.value+1}
				class="paginator-field"
				name="paginator-field"
				style={`--pageWidth=${pageWidth}`}
				onblur={(event: InputEventFor<VscodeTextfield>) => event.currentTarget.value && meltPage.set(parseInt(event.currentTarget.value, 10))}
				onkeyup={(event: KeyboardEvent) => event.key === "Enter" && meltPage.set(parseInt((event.currentTarget as HTMLInputElement).value, 10))}
			>    
			</vscode-textfield>
		{:else }
			<button use:melt={$pageTrigger(page)} class="pageButton flex flex-row justify-center">
				<vscode-badge variant="counter" class:badge-selected={page.value === currentPage} >
					{page.value}
				</vscode-badge>
			</button>
		{/if}
    {/each}
	<button use:melt={$nextButton} aria-label="next page">
		{#if $nextButton.disabled}
			<vscode-icon action-icon name="chevron-right" disabled></vscode-icon>
		{:else}
			<vscode-icon action-icon name="chevron-right"></vscode-icon>
		{/if}
	</button>
    {#if showRange}
        <span class="min-w-max self-center">{$range.start ?? 0} - {$range.end ?? count} / {count}</span>
    {/if}
</nav>

<style>
    nav {
        width: clamp(1rem, var(--pageWidth) * 1.2rem, 100%);
    }
    .paginator-field {
        --input-min-width: 0.5rem;
        min-width: calc(var(--pageWidth) * 1.2rem);
        max-width: calc(var(--pageWidth) * 1.2rem);
        text-align: center;
    }
    .paginator-gap {
        min-width: calc(var(--pageWidth) * 1.2rem);
        max-width: calc(var(--pageWidth) * 1.2rem);
    }
    .pageButton {
        min-width: calc(var(--pageWidth) * 1.2rem);
    }
    .badge-selected {
        --vscode-badge-background: var(--vscode-button-background);
        --vscode-badge-foreground: var(--vscode-button-foreground);
    }
</style>