<script lang="ts">
import type {
	VscodeSingleSelect,
	VscodeTextfield,
} from "@vscode-elements/elements";
import { onMount, type Snippet } from "svelte";
import type { HTMLAttributes } from "svelte/elements";
import type { SvelteHistory } from "$lib/base/svelteHistory.svelte";
// import VsCodeTooltipButton from "$lib/vscode/VSCodeTooltipButton.svelte";
import type { InputEventFor, MaybeAsync } from "$lib/types.d.ts";
import VsCodeTooltipButton from "$lib/vscode/VSCodeTooltipButton.svelte";
import CasSnippet from "./CasSnippet.svelte";
import type { CASResults, historyEntry } from "./types";

interface Props extends HTMLAttributes<HTMLElement> {
	perPage: number;
	debounceTime?: number;
	etime_sort?: boolean;
	waiting?: boolean;
	command: string;
	runCommand: (
		command: string,
		signal: AbortSignal,
	) => MaybeAsync<boolean | CASResults>;
	endButtons: Snippet;
	previousCommand?: () => MaybeAsync<unknown>;
	nextCommand?: () => MaybeAsync<unknown>;
	history: SvelteHistory<historyEntry>;
}

let {
	perPage = $bindable(50),
	debounceTime = 500,
	etime_sort = false,
	waiting = $bindable(false),
	command = $bindable(),
	runCommand,
	endButtons,
	previousCommand,
	nextCommand,
	history = $bindable(),
	...props
}: Props = $props();

let abort = new AbortController();
export async function runCmd() {
	waiting = true;
	abort.abort("started new search");
	abort = new AbortController();
	const success = await runCommand(command, abort.signal);
	waiting = false;
	clearTimeout(saveTimeout);
}
let saveTimeout: NodeJS.Timeout | undefined = undefined;
let backStack = $state(0);

let combobox = $state<VscodeSingleSelect | undefined>(undefined);
let textbox = $state<VscodeTextfield | undefined>(undefined);
let suggestions = $derived<historyEntry[]>(
	history.internalArray.filter((e) => e.length && e[0].length) ?? [],
);
$inspect(suggestions);

$effect(() => console.log(`commandPerPage: ${perPage}`));
$effect(() => {
	if (combobox) {
		combobox.options = suggestions.map((s) => ({
			label: s[0],
			value: s[0],
			description: s[0],
			disabled: false,
			selected: false,
		}));
		combobox.requestUpdate();
	}
});
</script>




<search 
  class="sticky top-0 z-10 flex flex-row w-full"
  {...props}
>
  <fieldset class="grid items-start z-10 min-w-full border-vscode-input-border border dark:border-0  bg-vscode-input-background text-vscode-input-foreground">
    <vscode-single-select 
      combobox
      bind:this={combobox}
      id="commandBox"
      value={command}
      onchange={(e: InputEventFor<VscodeSingleSelect>) => suggestions[e.currentTarget.selectedIndex][0].startsWith(command) ? command = suggestions[e.currentTarget.selectedIndex][0] : undefined}
      class="layer0 w-full"
      placeholder="CAS Command"
      type="text"
      filter="startsWith"
    >
      {#each suggestions as [commandOption]}
          <vscode-option>{commandOption}</vscode-option>
      {/each}
    </vscode-single-select>
    <div class="flex flex-row w-full layer1 z-10 pr-2 align-baseline">

    <input class="w-full bg-vscode-input-background text-vscode-input-foreground pl-1.5"
      bind:value={command}
      placeholder="command"
      onkeydown={(e: KeyboardEvent) => {
        if (!["Enter", "ArrowUp", "ArrowDown", "ArrowRight"].includes(e.key) || !combobox) {
          return;
        }
        e.preventDefault();
        if (e.key === "Enter") {
          // @ts-ignore
          if (combobox && suggestions.length && combobox._activeIndex >= 0 ) {
            // @ts-ignore
            command = combobox._filteredOptions.at(combobox._activeIndex).value;
          }
          setTimeout(() => runCmd(), 25);
        }
        // @ts-ignore
        if (combobox && e.key === "ArrowRight" && combobox._activeIndex >= 0) {
          // @ts-ignore
          command = combobox._filteredOptions.at(combobox._activeIndex).value;
        }
          const key = e.key === "ArrowRight" ? " " : e.key;
          const newEv = new KeyboardEvent(e.type, {
            key: key,
            code: e.code,
            repeat: e.repeat,
            detail: e.detail,
            ctrlKey: e.ctrlKey,
            metaKey: e.metaKey,
            altKey: e.altKey,
            shiftKey: e.shiftKey,
            location: e.location,
          })
        combobox.dispatchEvent(newEv)
        // @ts-ignore protected doesn't matter and this is the only way that works
        
      }}
      oninput={(e) => {
        command=e.currentTarget.value; 
        if (combobox && suggestions.length) {
          // @ts-ignore protected doesn't matter and this is the only way that works
          combobox._toggleDropdown(true)
          // @ts-ignore protected doesn't matter and this is the only way that works
          setTimeout(() => {
            // @ts-ignore protected doesn't matter and this is the only way that works
            combobox!._onSlotChange();
            // @ts-ignore protected doesn't matter and this is the only way that works
            combobox!._onComboboxInputInput(e);
          }, 10);
        }}}>

      <span class="flex flex-row align-baseline bg-vscode-input-background">
        <VsCodeTooltipButton
          description="Run Command"
          placement="bottom"
          class="pt-0.5"
          onclick={runCmd}
          vsbutton={false}
        >
            {#if waiting}
              <vscode-progress-ring class="h-4 w-4"></vscode-progress-ring>
            {:else}
              <vscode-icon name="run-all" class="run-icon"></vscode-icon>
            {/if}
        </VsCodeTooltipButton>
        <CasSnippet {command}></CasSnippet>
          <VsCodeTooltipButton
            description="Previous Command"
            placement="bottom"
            onclick={() => {
              previousCommand ? previousCommand() : {};
              backStack += 1;
            }}
            name="debug-step-back"
            disabled={!history.length || undefined}
          >
          </VsCodeTooltipButton>
          <VsCodeTooltipButton
            description="Next Command"
            placement="bottom"
            onclick={() => {
              nextCommand ? nextCommand() : {};
              backStack -= 1;
            }}
            icon="debug-step-over"
            disabled={!backStack || undefined}
          >
          </VsCodeTooltipButton>
        
        <vscode-single-select
          class="w-16"
          id="perPage"
          position="below"
          value={perPage.toString()}
          onvsc-change={(e: InputEvent) => {console.log("changing perPage"); perPage = parseFloat((e.currentTarget as VscodeSingleSelect).value)}}
          >
          <vscode-option value="10">10</vscode-option>
          <vscode-option value="50">50</vscode-option>
          <vscode-option value="100">100</vscode-option>
          <vscode-option value="1000">1000</vscode-option>
          <!-- <vscode-option value="Infinity">all</vscode-option> -->
        </vscode-single-select>
        {@render endButtons()}
      </span>
    </div>
  </fieldset>
</search>

<style>
  .run-icon {
    --vscode-icon-foreground: var(--vscode-debugIcon-startForeground)
  }
  .layer0, .layer1 {
    grid-column: 1;
    grid-row: 1;
  }
</style>