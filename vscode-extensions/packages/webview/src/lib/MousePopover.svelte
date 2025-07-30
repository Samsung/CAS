<script lang="ts">
import { onDestroy, onMount, type Snippet } from "svelte";
import { browser } from "$app/environment";

interface props {
	children: Snippet;
	visible: boolean;
}
const { children, visible }: props = $props();

let pointerEl = $state<HTMLElement>();
let popoverEl = $state<HTMLElement>();
$effect(() => {
	popoverEl?.togglePopover(visible);
});

if (browser) {
	document.addEventListener("mousemove", (e) => {
		pointerEl?.style.setProperty(
			"--mouse-x",
			(e.clientX + (document.scrollingElement?.scrollLeft ?? 0)).toString(),
		);
		pointerEl?.style.setProperty(
			"--mouse-y",
			(e.clientY + (document.scrollingElement?.scrollTop ?? 0)).toString(),
		);
	});
}
// function setPopoverSize() {
// 	const size = popoverEl?.getBoundingClientRect();
// 	popoverEl?.style.setProperty("--el-width", `${size?.width}px`);
// 	popoverEl?.style.setProperty("--el-height", `${size?.height}px`);
// }

// let observer = new MutationObserver(setPopoverSize);
// onMount(() => {
// 	if (popoverEl) {
// 		// observer.observe(popoverEl, {
// 		// 	subtree: true,
// 		// 	attributeFilter: ["innerText", "innerHTML", "clientWidth"],
// 		// });
// 		// setPopoverSize();
// 	}
// });
// onDestroy(observer.disconnect);
</script>


<div class="pointer" bind:this={pointerEl}></div>
<div popover bind:this={popoverEl} class="popover bg-vscode-panel-background border-vscode-panel-border border text-vscode-foreground">
    {@render children?.()}
</div>

<style>
	.pointer {
		position: fixed;
		width: 0;
		height: 0;
		max-width: 0;
		max-height: 0;
		top: calc(var(--mouse-y) * 1px);
		left: calc(var(--mouse-x) * 1px);
		anchor-name: --pointer;
	}
	.popover {
        --el-width: 25rem;
        --el-height: 2rem;
        z-index: 9999;
		position: fixed;
		position-anchor: --pointer;
		top: min(calc(anchor(top) + 8px), calc(100vh - var(--el-height)));
		margin: 0px;
		left: min(calc(anchor(left) + 8px), calc(100vw - var(--el-width)));
		right: anchor(right);
		bottom: anchor(bottom);
		pointer-events: none;
		word-wrap: break-word;
        width: var(--el-width);
	}
</style>