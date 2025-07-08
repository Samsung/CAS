<script module lang="ts">
type SearchMode = "wc" | "re" | "m";
type FilterField = "bin" | "cwd" | "cmd";
type ClassQuery = "command" | "linker" | "compiler";

type SearchFilter = {
	[Key in FilterField]?: FilterQuery;
};
interface FilterQuery {
	value: string;
	type: SearchMode;
}
export interface SearchQuery {
	filename?: string;
	filter?:
		| SearchFilter
		| {
				negate?: boolean;
				bin_source_root?: boolean;
				class?: ClassQuery;
		  };
	etime_sort?: boolean;
}
export function queryToSearch(query: SearchQuery): URLSearchParams {
	let search = new URLSearchParams();
	if (query.filter) {
		const filter = Object.entries(query.filter)
			.map(([key, filter]) => {
				if (!filter) {
					return;
				}
				if (typeof filter !== "object") {
					return `${key}=${filter}`;
				}
				return `${key}=${filter.value},type=${filter.type}`;
			})
			.filter((v) => typeof v === "string");
		search.append("filter", `[${filter.join(",")}]`);
	}
	if (query.filename) {
		search.append("filename", query.filename);
	}
	if (query.etime_sort) {
		search.append("etime_sort", "true");
	}
	return search;
}
</script>
<script lang="ts">
  import type { Process } from "@cas/types/bas.js";
  import type { MaybeAsync } from "$lib/types.d.ts";
  import type { HTMLAttributes } from 'svelte/elements';
  import { debounce } from "lodash-es"
  import VsCodeTooltipButton from "$lib/vscode/VSCodeTooltipButton.svelte";
  import { stopPropagation } from "$lib/helpers";
  import * as t from "$lib/paraglide/messages.js";
  import type { VscodeTextfield } from "@vscode-elements/elements";
    interface Props extends HTMLAttributes<HTMLElement> {
      search: (query: SearchQuery, signal: AbortSignal) => MaybeAsync<Process[]>;
      searchResults?: Process[];
      debounceTime?: number;
      etime_sort?: boolean;
      filter?: boolean;
    }

    let {
      search,
      searchResults = $bindable([]),
      debounceTime = 500,
      etime_sort = $bindable(false),
      filter = $bindable(false),
      ...props
    }: Props = $props();

    let waiting = $state(false);
    let searchBox: VscodeTextfield;

    let abort = new AbortController()
    async function runSearch() {
      waiting = true;
      abort.abort("started new search");
      abort = new AbortController();
      searchResults = await search({
        filter: searchBox.value.length || negated || source_root || classQuery !== "*" ? {
          [field]: searchBox.value.length ? {
            type: mode,
            value: searchBox.value
          }: undefined,
          negate: negated,
          bin_source_root: source_root,
          class: classQuery === "*" ? undefined : classQuery,
        } : undefined,
        filename: pathQuery.length ? pathQuery : undefined,
      }, abort.signal)
      waiting = false;
    }
    let mode = $state("wc") as SearchMode;
    let field = $state("bin") as FilterField;
    let advancedOpen = $state(false);
    let pathQuery = $state("");
    let classQuery = $state("*") as "*" | ClassQuery;
    let negated = $state(false);
    let source_root = $state(false);
    let sort = $state(false);
</script>


<search 
  class="sticky top-0 z-10 flex flex-row -ml-[22px] bg-vscode-panel-background max-h-full"
  {...props}
>
  <VsCodeTooltipButton
    onclick={stopPropagation(() => advancedOpen = !advancedOpen)}
    description="Advanced"
    icon={advancedOpen ? "chevron-down" : "chevron-right"}
  >
  </VsCodeTooltipButton>

  <fieldset class="flex flex-col grow max-h-full">
    <vscode-textfield 
      bind:this={searchBox}
      oninput={debounce(runSearch, debounceTime)} 
      onkeyup={(e: KeyboardEvent) => {if (e.key === "Enter") runSearch()}}
      class="grow min-w-full"
      placeholder={t.search()}
    >
        <span slot="content-before" class="flex flex-row">
          {#if waiting}
          <VsCodeTooltipButton
            description="Search"
            placement="bottom"
            onclick={runSearch}
          >
                <vscode-progress-ring class="h-4 w-4"></vscode-progress-ring>
              </VsCodeTooltipButton>
              {:else}
              <VsCodeTooltipButton
                description="Search"
                placement="bottom"
                onclick={runSearch}
                icon="search"
              >
              </VsCodeTooltipButton> 
              {/if}
        </span>
        <span slot="content-after" class="flex flex-row">
          <VsCodeTooltipButton
            class={negated ? `bg-vscode-inputOption-activeBackground text-vscode-inputOption-activeForeground border-vscode-inputOption-activeBorder` : ''}
            onclick={() => negated = !negated}
            description="Negate Filter"
            placement="bottom"
            name="negate"
          >          
              <span class="font-bold">!</span>
          </VsCodeTooltipButton>
          <hr class="mx-1 border-vscode-input-foreground border-l h-4/5 my-auto"/>
          <fieldset name="searchField" class="flex flex-row">
            <VsCodeTooltipButton
            class={field === "bin" ? `bg-vscode-inputOption-activeBackground text-vscode-inputOption-activeForeground border-vscode-inputOption-activeBorder` : ''}
            onclick={() => field = "bin"}
            value="bin"
            description="bin"
            placement="bottom"
            name="bin"
            >
                <label for="bin">bin</label>
            </VsCodeTooltipButton>
            <VsCodeTooltipButton
            class={field === "cwd" ? `bg-vscode-inputOption-activeBackground text-vscode-inputOption-activeForeground border-vscode-inputOption-activeBorder` : ''}
            onclick={() => field = "cwd"}
            value="cwd"
            description="cwd"
            placement="bottom"
            name="cwd"
            >
                <label for="cwd">cwd</label>
            </VsCodeTooltipButton>
            <VsCodeTooltipButton
            class={field === "cmd" ? `bg-vscode-inputOption-activeBackground text-vscode-inputOption-activeForeground border-vscode-inputOption-activeBorder` : ''}
            onclick={() => field = "cmd"}
            value="cmd"
            description="cmd"
            placement="bottom"
            name="cmd"
            >
                <label for="cmd">cmd</label>
            </VsCodeTooltipButton>
          </fieldset>
          <hr class="mx-1 border-vscode-input-foreground border-l h-4/5 my-auto"/>
          <fieldset name="searchMode" class="flex flex-row">
            <VsCodeTooltipButton
            class={mode === "wc" ? `bg-vscode-inputOption-activeBackground text-vscode-inputOption-activeForeground border-vscode-inputOption-activeBorder` : ''}
            onclick={() => mode = "wc"}
            value="wc"
            description="Wildcard"
            placement="bottom"
            icon="search-fuzzy"
            >
            </VsCodeTooltipButton>
            <VsCodeTooltipButton
              class={mode === "re" ? `bg-vscode-inputOption-activeBackground text-vscode-inputOption-activeForeground border-vscode-inputOption-activeBorder` : ''}
              onclick={() => mode = "re"}
              value="re"
              description="Regular Expression"
              placement="bottom"
              icon="regex"
            >
            </VsCodeTooltipButton>
            <VsCodeTooltipButton
              class={mode === "m" ? `bg-vscode-inputOption-activeBackground text-vscode-inputOption-activeForeground border-vscode-inputOption-activeBorder` : ''}
              onclick={() => mode = "m"}
              value="m"
              description="Exact Match"
              placement="bottom"
              icon="whole-word"
            >
            </VsCodeTooltipButton>
          </fieldset>
        </span>
    </vscode-textfield>
    <fieldset class:hidden={!advancedOpen}>
      <vscode-textfield 
        onchange={(e: InputEvent) => pathQuery = (e.target as HTMLInputElement).value}
        onkeyup={(e: KeyboardEvent) => {if (e.key === "Enter"){runSearch()} }}
        class={`sticky top-0 z-10 grow`}
        placeholder="Absolute path"
      >
      Opened path
      </vscode-textfield>
      <fieldset class="flex flex-row flex-wrap justify-between">

        <vscode-radio-group 
          name="class"
          orientation="horizontal"
          value="*"
          onchange={(e: InputEvent) => {
            classQuery = (e.target as HTMLInputElement).value as "*" | ClassQuery;
            runSearch();
          }}
        >
          <label slot="label" for="class">Class</label>
          <vscode-radio value="*">Any</vscode-radio>
          <vscode-radio value="command">Command</vscode-radio>
          <vscode-radio value="linker">Linker</vscode-radio>
          <vscode-radio value="compiler">Compiler</vscode-radio>
        </vscode-radio-group>
        <fieldset class="mt-8">
          <vscode-checkbox onchange={(e: InputEvent) => {
            source_root = (e.target as HTMLInputElement).checked;
            runSearch();
          }}>
            Source root
          </vscode-checkbox>
        </fieldset>
      </fieldset>
    </fieldset>
  </fieldset>
  <VsCodeTooltipButton
    description="Sort by Etime"
    class={etime_sort ? `bg-vscode-inputOption-activeBackground text-vscode-inputOption-activeForeground border-vscode-inputOption-activeBorder` : ''}
    onclick={() => etime_sort = !etime_sort}
    icon="list-ordered"
  >
  </VsCodeTooltipButton>
  <VsCodeTooltipButton
    description="Filter Empty"
    class={filter ? `bg-vscode-inputOption-activeBackground text-vscode-inputOption-activeForeground border-vscode-inputOption-activeBorder` : ''}
    onclick={() => filter = !filter}
    icon={filter ? "filter-filled" : "filter"}
  >
  </VsCodeTooltipButton>
</search>
