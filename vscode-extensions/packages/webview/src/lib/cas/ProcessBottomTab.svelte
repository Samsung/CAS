<script lang="ts">
import { nsFormat } from "$lib/helpers";
import * as t from "$lib/paraglide/messages";
import type { ProcessInfoEventData, ProcNode } from "$lib/types";
import { vscode } from "$lib/vscode";
import VsCodePagedTable from "$lib/vscode/VSCodePagedTable.svelte";
import type {
	FileMode,
	ProcessInfo,
	ProcessWithChildren,
} from "@cas/types/bas.js";
import { createDialog, melt } from "@melt-ui/svelte";
import { copy } from "@svelte-put/copy";
import { join as shJoin } from "shlex";

interface Props {
	node: ProcessWithChildren;
	open?: boolean;
	showButton?: boolean;
	nameFilter?: RegExp;
}
let {
	node: rawNode,
	open: forceOpen = $bindable(),
	showButton = true,
	nameFilter,
}: Props = $props();

const node = $derived<ProcNode>({
	parent: null,
	eid: { pid: rawNode.pid, idx: rawNode.idx },
	data: {
		...rawNode,
		children: Array.isArray(rawNode.children)
			? rawNode.children.length
			: rawNode.children,
	},
	children: [],
	hasChildren: Array.isArray(rawNode.children) || !!rawNode.children,
	expanded: false,
	hasParent: false,
});

const {
	elements: { trigger, overlay, content, title, description, close, portalled },
	states: { open },
} = createDialog({
	forceVisible: true,
	onOpenChange({ curr, next }) {
		if (next !== forceOpen) {
			forceOpen = next;
		}
		return next;
	},
	escapeBehavior: "ignore",
	closeOnOutsideClick: false,

	defaultOpen: $state.snapshot(forceOpen),
});
async function loadInfo(page?: number): Promise<ProcessInfo> {
	const { data } = (await vscode.execFunction("getProcessInfo", {
		eid: $state.snapshot(node.eid),
		page,
	})) as ProcessInfoEventData;
	return data;
}
let additionalOpens: { filename: string; mode: FileMode }[] = $state([]);
function padToPage(items: unknown[], page: number, perPage: number = 25) {
	return new Array(page * perPage).concat(items);
}
let expanded = $state<Record<string, boolean>>({});
$effect(() => {
	if (forceOpen) {
		$open = true;
	}
});
</script>


    {#if showButton}
    <vscode-icon action-icon name="info" use:melt={$trigger}></vscode-icon>
    {/if}
    {#if $open}
      <div use:melt={$portalled}>
        <div
          use:melt={$content}
          class="fixed left-1/2 bottom-0 z-50 max-h-[95vh] w-[90vw]
                max-w-[1000px] -translate-x-1/2 rounded-xl
                bg-vscode-panel-background border-vscode-panel-border
                p-6 shadow-lg
                overflow-y-auto
                modal
                "
        >
          <h2 use:melt={$title} class="font-extrabold text-lg">
            {t.processDetails(node.eid)}
          </h2>
          <div use:melt={$description}>
            <vscode-table>
              <vscode-table-body slot="body">
                <vscode-table-row>
                  <vscode-table-cell  use:copy={{text: `${node.eid.pid}:${node.eid.idx}`}}>
                    {t.process()}: {node.eid.pid}:{node.eid.idx}
                  </vscode-table-cell>
                  {#if node.data.etime}
                  <vscode-table-cell use:copy={{text: nsFormat(node.data.etime) ?? ""}} onclick={() => console.log(node)}>
                      {t.duration()}: {nsFormat(node.data.etime, node.data.etime > 1e9 ? "millisecond" : "nanosecond")}
                    </vscode-table-cell>
                  {/if}
                  <vscode-table-cell  use:copy={{text: `${node.data.ppid}:${node.data.pidx}`}}>
                    {t.parent()}: {node.data.ppid}:{node.data.pidx}
                  </vscode-table-cell>
                  <vscode-table-cell  use:copy={{text: node.data.children.toFixed(0)}}>
                    {t.children()}: {node.data.children}
                  </vscode-table-cell>
                </vscode-table-row>
              </vscode-table-body>
            </vscode-table>
    
            <vscode-table columns={["20px", "auto"]} minColumnWidth="5px" responsive={true}>
              <vscode-table-body slot="body">
                <vscode-table-row class="flex flex-row">
                  <vscode-table-cell class="max-w-20 w-16">CWD</vscode-table-cell>
                  <vscode-table-cell class="grow w-full h-auto" use:copy class:text-pretty={!!expanded.cwd}>
                    <span onclick={() => window.getSelection()?.type !== "Range" ? expanded.cwd = !expanded.cwd : undefined}>{node.data.cwd}</span>
                  </vscode-table-cell>
                </vscode-table-row>
                <vscode-table-row class="flex flex-row">
                  <vscode-table-cell  class="max-w-20 w-16">BIN</vscode-table-cell>
                  <vscode-table-cell  class="grow w-full h-auto" use:copy class:text-pretty={!!expanded.bin}>
                    <span onclick={() => window.getSelection()?.type !== "Range" ? expanded.bin = !expanded.bin : undefined}>{node.data.bin}</span>
                  </vscode-table-cell>
                </vscode-table-row>
                <vscode-table-row class="flex flex-row">
                  <vscode-table-cell class="max-w-20 w-16">CMD</vscode-table-cell>
                  <vscode-table-cell  class="grow w-full h-auto" use:copy class:text-pretty={!!expanded.cmd}>
                    <span onclick={() => window.getSelection()?.type !== "Range" ? expanded.cmd = !expanded.cmd : undefined}>{shJoin(node.data.cmd)}</span>
                  </vscode-table-cell>
                </vscode-table-row>
              </vscode-table-body>
            </vscode-table>
            {#await loadInfo()}
              <div class="flex flex-row justify-center">
                <vscode-progress-ring></vscode-progress-ring>
              </div>
            {:then info}
              {#snippet openfiles()}
                {#if info.open_len > 0 && info.openfiles.length}
                  <vscode-collapsible title={t.openedFiles()}>
                    <vscode-badge variant="counter" slot="decorations">{info.open_len}</vscode-badge>
                    
                    <VsCodePagedTable
                      rowsData={info.openfiles.map(([mode, filename]) => ({filename, mode})).concat(additionalOpens)}
                      columns={["auto", "4rem"]}
                      searchable={true}
                      itemCount={info.open_len}
                      perPage={25}
                      pages={Math.ceil(info.open_len/25)}
                      onPageChange={async (e) => {
                        additionalOpens = padToPage(
                            (await loadInfo(Math.min(Math.max(Math.ceil((e.detail) / 2) - 1, 0), info.files_pages - 1)))
                              .openfiles
                              .map(([mode, filename]) => ({filename, mode})),
                              Math.min(Math.max(Math.ceil(e.detail / 2) - 2,0), info.files_pages - 1),
                            50
                          );
                        }}
                      onSearch={async (term, page) => {
                        additionalOpens = [];
                        // load up to 50 pages = 1000 files
                        for (let i = 0; i<Math.min(50, info.files_pages-page); i++) {
                          additionalOpens.push(...(await loadInfo(Math.ceil(page/4)+i)).openfiles.map(([mode, filename]) => ({filename, mode})));
                        }
                      }}
                    >
                      {#snippet renderItem(value, key, item)}
                        <span
                          class:text-vscode-list-warningForeground={item.mode === "rw"}
                          class:text-vscode-list-errorForeground={item.mode === "w"}
                        >
                          {value}
                        </span>
                      {/snippet}
                    </VsCodePagedTable>
                  </vscode-collapsible>
                {/if}
              {/snippet}
              {#if info.class === "compiler"}
                {@render openfiles()}
                <vscode-collapsible title={t.compilerInfo()}>
                  <div class="pl-4">
                    <h3 class="font-bold">{t.compiled()}</h3>
                    <VsCodePagedTable
                      rowsData={info.compiled.map(c => ({type: Object.values(c)[0], path: Object.keys(c)[0]}))}
                      columns={["56px", "auto"]}
                    >
                      {#snippet renderItem(value, key)}
                        {#if key == "type"}
                          <div class="flex flex-row justify-center">
                            <vscode-badge>{value}</vscode-badge>
                          </div>
                        {:else}
                          {value}
                        {/if}
                      {/snippet}
                    </VsCodePagedTable>
                    <h3 class="font-bold">{t.objects()}</h3>
                    <VsCodePagedTable
                      rowsData={info.objects.map(c => ({path: c}))}
                      generate-header="none"
                    >
                    </VsCodePagedTable>
    
                    {#if info.headers?.length}
                      <vscode-collapsible title="headers">
                        <vscode-badge variant="counter" slot="decorations">{info.headers.length}</vscode-badge>
                        <VsCodePagedTable
                          rowsData={info.headers.map(path => ({path}))}
                          generate-header="none"
                          searchable={true}
                        ></VsCodePagedTable>
                      </vscode-collapsible>
                    {/if}
    
                    {#if info.ipaths?.length}
                      <vscode-collapsible title="Ipaths">
                        <vscode-badge variant="counter" slot="decorations">{info.ipaths.length}</vscode-badge>
    
                        <VsCodePagedTable
                          rowsData={info.ipaths.map(path => ({path}))}
                          generate-header="none"
                          searchable={true}
                        ></VsCodePagedTable>
                      </vscode-collapsible>
                    {/if}
                    {#if info.defs?.length}
                      <vscode-collapsible title="Defs">
                        <vscode-badge variant="counter" slot="decorations">{info.defs.length}</vscode-badge>
    
                        <VsCodePagedTable
                          rowsData={info.defs.map(([variable, value]) => ({def: `${variable}=${value}`}))}
                          generate-header="none"
                          searchable={true}
                          ></VsCodePagedTable>
                      </vscode-collapsible>
                    {/if}
                    {#if info.undefs?.length}
                      <vscode-collapsible title="Undefs">
                        <vscode-badge variant="counter" slot="decorations">{info.undefs.length}</vscode-badge>
    
                        <VsCodePagedTable
                        rowsData={info.undefs.map(([variable, value]) => ({undef: `${variable}=${value}`}))}
                        generate-header="none"
                        searchable={true}
                        ></VsCodePagedTable>
                      </vscode-collapsible>
                    {/if}
    
                    </div>
                </vscode-collapsible>
              {:else if info.class === "linker"}
    
                  {@render openfiles()}
                <vscode-collapsible title={t.linker()}>
                  <vscode-badge variant="counter" slot="decorations">{info.linked ? 1 : 0}</vscode-badge>
                  <p>{info.linked}</p>
                </vscode-collapsible>
              {:else}
                {@render openfiles()}
              {/if}
            {/await}
          </div>
          <div class="flex flex-row justify-between mt-4">
            <div class="flex flex-row justify-start gap-2">
              <vscode-button  onclick={() => vscode.execFunction("saveJSON", { eid: $state.snapshot(node.eid) })}>
                Save to JSON
              </vscode-button>
              <vscode-button onclick={() => vscode.execFunction("openProcTimeGraph", { data: { pid: rawNode.pid, idx: rawNode.idx}})}>
                Show process timing
              </vscode-button>
            </div>
            <!-- <vscode-button secondary use:melt={$close}
              >{t.close()}</vscode-button
            > -->
          </div>
        </div>
      </div>
    {/if}
    
    <style>
      .header-counter {
        grid-template-columns: 5.25rem max-content;
      }
      .header-counter-compiler {
        grid-template-columns: 3.5rem max-content;
      }
    </style>