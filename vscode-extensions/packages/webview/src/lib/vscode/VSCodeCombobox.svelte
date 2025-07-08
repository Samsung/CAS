<script lang="ts">
import type { VscodeTextfield } from "@vscode-elements/elements";
import type { Snippet } from "svelte";
import type { HTMLInputAttributes } from "svelte/elements";

interface Props extends HTMLInputAttributes {
	label?: string;
	value?: string;
	children?: Snippet;
	start?: Snippet;
	end?: Snippet;
	suggestions?: string[];
}
let {
	label,
	value = $bindable(),
	suggestions = $bindable([]),
	children,
	start,
	end,
	...props
}: Props = $props();
</script>

<div class="flex flex-col gap-1">
    <div
        class="grid grid-cols-1"
    >
        <vscode-textfield
            {value}
            oninput={(e: InputEvent) => value = (e.currentTarget as VscodeTextfield).value}
            id="control"
            {...props}
            aria-label={label}
            class="w-full layer1"
        >
            {#if start}
                <span slot="content-before" class="flex m-auto fill-current">
                    {@render start()}
                </span>
            {/if}
                    
            {#if children}
                {@render children()}
            {/if}
            {#if end}
                <span slot="content-after" class="flex m-auto fill-current">
                    {@render end()}
                </span>
            {/if}
        </vscode-textfield>
        <vscode-single-select combobox >

        </vscode-single-select>
    </div>
</div>

<style>

</style>