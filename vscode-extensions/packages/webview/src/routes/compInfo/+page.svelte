<script lang="ts">
import { copy } from "@svelte-put/copy";
import { onMount } from "svelte";
import type { ElementTypes } from "$lib/cas/CasResults.svelte";
import * as t from "$lib/paraglide/messages";
import { type CasApiEvent, vscode } from "$lib/vscode";
import VsCodePagedTable from "$lib/vscode/VSCodePagedTable.svelte";

interface CompilationInfo {
	command: {
		pid: number;
		idx: number;
		ppid: number;
		pidx: number;
		bin: string;
		cwd: string;
		command: string;
	};
	source_type: string;
	compiled_files: string[];
	include_paths: string[];
	headers: string[];
	defs: {
		name: string;
		value: string;
	}[];
	undefs: {
		name: string;
		value: "";
	}[];
}

interface InitEvent extends CasApiEvent {
	data: {
		info: CompilationInfo;
	};
}
let info = $state<CompilationInfo | undefined>(undefined);

function getVSCodeContext(v: unknown, type: ElementTypes) {
	return JSON.stringify({
		elem_type: type,
		v,
	});
}

onMount(async () => {
	({
		data: { info },
	} = (await vscode.execFunction("init", {})) as InitEvent);
	vscode.addEventListener(
		"init",
		(event: MessageEvent<{ data: { info: CompilationInfo } }>) =>
			(info = event.data.data.info),
	);
});

let expanded = $state<Record<string, boolean>>({ cmd: false });
</script>
<vscode-table>
    <vscode-table-body slot="body">
        <vscode-table-row>
            <vscode-table-cell  use:copy={{text: `${info?.command.pid}:${info?.command.idx}`}} data-vscode-context={getVSCodeContext(`${info?.command.ppid}:${info?.command.pidx}`, "pid")} >
            {t.process()}: {info?.command.pid}:{info?.command.idx}
            </vscode-table-cell>
            <vscode-table-cell  use:copy={{text: `${info?.command.ppid}:${info?.command.pidx}`}} data-vscode-context={getVSCodeContext(`${info?.command.ppid}:${info?.command.pidx}`, "pid")}>
            {t.parent()}: {info?.command.ppid}:{info?.command.pidx}
            </vscode-table-cell>
        </vscode-table-row>
    </vscode-table-body>
</vscode-table>

<vscode-table columns={["20px", "auto"]} minColumnWidth="5px" responsive={true}>
    <vscode-table-body slot="body">
    <vscode-table-row class="flex flex-row">
        <vscode-table-cell class="max-w-20 w-16">CWD</vscode-table-cell>
        <vscode-table-cell class="grow w-full text-pretty h-auto" use:copy data-vscode-context={getVSCodeContext(info?.command.cwd, "path")}>
        {info?.command.cwd}
        </vscode-table-cell>
    </vscode-table-row>
    <vscode-table-row class="flex flex-row">
        <vscode-table-cell  class="max-w-20 w-16">BIN</vscode-table-cell>
        <vscode-table-cell  class="grow w-full text-pretty h-auto" use:copy data-vscode-context={getVSCodeContext(info?.command.bin, "bin")}>
        {info?.command.bin}
        </vscode-table-cell>
    </vscode-table-row>
        <vscode-table-row class="flex flex-row">
            <vscode-table-cell class="max-w-20 w-16">CMD</vscode-table-cell>
            <vscode-table-cell  class="grow w-full h-auto" class:text-pretty={expanded.cmd} use:copy data-vscode-context={getVSCodeContext(info?.command.command, "cmd")} onclick={() => window.getSelection()?.type !== "Range" ? expanded.cmd = !expanded.cmd : undefined}>
            {info?.command.command}
            </vscode-table-cell>
        </vscode-table-row>
    </vscode-table-body>
</vscode-table>

{#if info}
    <h3 class="font-bold">{t.compiled()}</h3>
    <VsCodePagedTable
        rowsData={info.compiled_files.map(v => ({ file: v}))}
        columns={["auto"]}
    >
        {#snippet renderItem(value, key)}
        <span data-vscode-context={getVSCodeContext(value, "compiled")}>
            {value}
        </span>
        {/snippet}
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

    {#if info.include_paths?.length}
        <vscode-collapsible title="Ipaths">
        <vscode-badge variant="counter" slot="decorations">{info.include_paths.length}</vscode-badge>

        <VsCodePagedTable
            rowsData={info.include_paths.map(path => ({path}))}
            generate-header="none"
            searchable={true}
        >
            {#snippet renderItem(value, key)}
            <span data-vscode-context={getVSCodeContext(value, "linked")}>
                {value}
            </span>
            {/snippet}
        </VsCodePagedTable>
        </vscode-collapsible>
    {/if}
    {#if info.defs?.length}
        <vscode-collapsible title="Defs">
        <vscode-badge variant="counter" slot="decorations">{info.defs.length}</vscode-badge>

        <VsCodePagedTable
            rowsData={info.defs}
            searchable={true}
            columns={["50%", "50%"]}
            ></VsCodePagedTable>
        </vscode-collapsible>
    {/if}
    {#if info.undefs?.length}
        <vscode-collapsible title="Undefs">
        <vscode-badge variant="counter" slot="decorations">{info.undefs.length}</vscode-badge>

        <VsCodePagedTable
        rowsData={info.undefs}
        searchable={true}
        columns={["50%", "50%"]}
        ></VsCodePagedTable>
        </vscode-collapsible>
    {/if}

{/if}
