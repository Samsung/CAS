<script lang="ts">
import type { ICellRendererParams } from "ag-grid-community";
import { onMount } from "svelte";
import type { ElementTypes } from "./CasResults.svelte";

let {
	value,
	context,
	colDef,
	node,
	column,
}: ICellRendererParams<
	any,
	any,
	{
		elem_type: ElementTypes;
	}
> = $props();

onMount(() => {
	if (typeof value === "object") {
		node.setRowHeight(
			((column?.getActualWidth() ?? 100) / 5) *
				JSON.stringify(value, null, 2).length,
		);
	}
});
</script>

<div data-vscode-context={JSON.stringify({
    elem_type: context?.elem_type ?? colDef?.context?.elem_type ?? "path",
    v: value
})}>
	{#if typeof value === "object"}
		{JSON.stringify(value, null, 2)}
	{:else}
	    {value}
	{/if}
</div>