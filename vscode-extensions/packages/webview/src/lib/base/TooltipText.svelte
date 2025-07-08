<script lang="ts">
import { createTooltip, melt } from "@melt-ui/svelte";
import type { Snippet } from "svelte";
import type { HTMLAttributes } from "svelte/elements";
import { fade } from "svelte/transition";

interface Props extends HTMLAttributes<HTMLSpanElement> {
	children: Snippet;
	description: string;
	placement?:
		| "top"
		| "top-start"
		| "top-end"
		| "right"
		| "right-start"
		| "right-end"
		| "bottom"
		| "bottom-start"
		| "bottom-end"
		| "left"
		| "left-start"
		| "left-end";
}
const {
	children,
	description = $bindable(),
	placement = $bindable("bottom"),
	...props
}: Props = $props();

const {
	elements: { trigger, content, arrow },
	states: { open },
} = createTooltip({
	positioning: {
		placement,
	},
	openDelay: 250,
	closeDelay: 50,
	closeOnPointerDown: false,
	forceVisible: true,
});
</script>
      
<span class="trigger" use:melt={$trigger} {...props}>
    {@render children()}
</span>

{#if $open}
<div
    use:melt={$content}
    transition:fade={{ duration: 100 }}
    class="z-50 rounded-[4px] border bg-vscode-editorWidget-background border-vscode-editorWidget-border text-vscode-editorWidget-foreground"
>
    <div use:melt={$arrow}></div>
    <p class="px-1 py-0.5">{description}</p>
</div>
{/if}