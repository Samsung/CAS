<script lang="ts">
import { createPagination, melt, type PageItem } from "@melt-ui/svelte";
import type {
	AnyMeltElement,
	ChangeFn,
	MeltElement,
} from "@melt-ui/svelte/internal/helpers";
import type { Snippet } from "svelte";

interface Props {
	pageButton: Snippet<[number, AnyMeltElement, boolean]>;
	previousPageButton: Snippet<[AnyMeltElement]>;
	nextPageButton: Snippet<[AnyMeltElement]>;
	pageInput: Snippet<[ChangeFn<number>, number, number]>;
	onChange?: (event: UIEvent) => unknown;
	page: number;
	pages: number;
	ellipsis: boolean;
	siblingCount?: number;
}
let {
	pageButton,
	previousPageButton,
	nextPageButton,
	onChange,
	pageInput,
	page = $bindable(1),
	pages: pageCount,
	ellipsis = true,
	siblingCount = 1,
}: Props = $props();
let Paginator;
const goToPage: ChangeFn<number> = ({ curr, next }) => {
	if (next > pageCount || next < 1 || isNaN(next)) {
		return curr;
	}
	if (onChange) {
		onChange(
			new UIEvent("change", {
				cancelable: false,
				detail: page,
				bubbles: true,
			}),
		);
	}
	return next;
};
function ellipsisPage(i: number, displayedPages: PageItem[]): number {
	const sign = Math.sign(i - 2);
	// assume we'll never have ellipsis two times in a row
	const displayedPage = displayedPages[i - sign];
	return ("value" in displayedPage ? displayedPage.value : page) + sign;
}
const {
	elements: { root, pageTrigger, prevButton, nextButton },
	states: { pages, range },
} = createPagination({
	count: pageCount,
	defaultPage: page,
	siblingCount,
});
</script>

<nav class="grid max-w-max gap-1" use:melt={$root}>

    {@render previousPageButton(prevButton)}
    {#each $pages as displayedPage, i}
        {#if displayedPage.type === 'ellipsis'}
            {@render pageInput(goToPage, page, ellipsisPage(i, $pages))}
        {:else}
            {@render pageButton(displayedPage.value, pageTrigger as unknown as AnyMeltElement, page === displayedPage.value)}
        {/if}
    {/each}
    {@render nextPageButton(nextButton)}

</nav>

<style>
    nav {
        grid-template-columns: repeat(11, max-content);
    }
</style>