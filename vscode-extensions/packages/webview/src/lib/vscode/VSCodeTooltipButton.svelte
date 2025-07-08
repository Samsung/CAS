<script lang="ts">
import { createTooltip, melt } from "@melt-ui/svelte";
import type { Snippet } from "svelte";
import type { HTMLButtonAttributes } from "svelte/elements";
import { fade } from "svelte/transition";

interface Props extends HTMLButtonAttributes {
	children?: Snippet;
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
	icon?: string;
	vsbutton?: boolean;
}
const {
	children,
	description = $bindable(),
	placement = $bindable("bottom"),
	icon,
	vsbutton = true,
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
  
	{#if children && vsbutton}
		<vscode-button type="button" {icon} secondary class="trigger" use:melt={$trigger} aria-label={description} {...props}>
			{@render children()}
		</vscode-button>
	{:else if children}
		<button class="trigger" use:melt={$trigger} aria-label={description} {...props} disabled={undefined}>
			{@render children()}
		</button>
	{:else}
		<vscode-icon name={icon} action-icon class="trigger" use:melt={$trigger} aria-label={description} {...props}>
		</vscode-icon>
	{/if}
  
  {#if $open}
    <div
      use:melt={$content}
      transition:fade={{ duration: 100 }}
      class="z-10 rounded-[4px] border bg-vscode-editorWidget-background border-vscode-editorWidget-border text-vscode-editorWidget-foreground"
    >
      <div use:melt={$arrow}></div>
      <p class="px-1 py-0.5">{description}</p>
    </div>
  {/if}