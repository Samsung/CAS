<script lang="ts">
import { SvelteHistory } from "$lib/base/svelteHistory.svelte";
import CasCommand from "$lib/cas/CasCommand.svelte";
import CasResults from "$lib/cas/CasResults.svelte";
import type { CASResults, EntryArray, historyEntry } from "$lib/cas/types";
import { vscode } from "$lib/vscode";
import { History } from "@cas/helpers/history.js";
import { promiseWithResolvers } from "@cas/helpers/promise.js";
import { isEqual } from "lodash-es";
import { onMount } from "svelte";

interface CasResultsEvent {
	func: string;
	id: string;
	data: CASResults;
}
interface CasInitEvent {
	func: string;
	id: string;
	data: {
		isGenerated: boolean;
		history: [string, number, number][];
		enabled: boolean;
	};
}
interface CasCmdEvent {
	func: string;
	id: String;
	data: { cmd: string; page?: number; entries?: number; saveHistory?: boolean };
}
let isGenerated = $state(false);
let enabled = $state(true);
let results = $state(undefined) as CASResults | undefined;
let perPage = $state(50);
let page = $state(1);
let command = $state("");
let selected: EntryArray = $state([]);
let history: SvelteHistory<historyEntry> = $state(
	new SvelteHistory<historyEntry>(undefined),
);
let casCommand;
onMount(async () => {
	let histArray;
	({
		data: { isGenerated, history: histArray, enabled },
	} = (await vscode.execFunction("init", {})) as CasInitEvent);
	history = new SvelteHistory(histArray);
	vscode.addEventListener("init", (event) => {
		if (
			event.data &&
			typeof event.data === "object" &&
			"enabled" in event.data
		) {
			enabled = !!event.data.enabled;
		}
		if (
			event.data &&
			typeof event.data === "object" &&
			"history" in event.data
		) {
			history = new SvelteHistory(event.data.history as historyEntry[]);
		}
	});
});

vscode.addEventListener("setCmdInput", (e) => {
	const event = e.data as CasCmdEvent;
	command = event.data.cmd;
	if (event.data.page) {
		page = event.data.page;
	}
	if (event.data.entries) {
		perPage = event.data.entries;
	}
	runCommand(command, undefined, true, true, event.data.saveHistory ?? true);
});
vscode.addEventListener("gatherSelected", (e) => {
	generateWorkspace(command, false);
});

let waiting = $state(false);
let done = Promise.resolve();
async function runCommand(
	cmd: string,
	signal?: AbortSignal,
	reload = true,
	setState = true,
	saveHistory = true,
) {
	await done;
	waiting = true;
	if (reload) {
		page = 1;
		selected = [];
	}
	const { resolve, promise } = promiseWithResolvers<void>(signal);
	done = promise;
	let data: CASResults;
	if (cmd) {
		({ data } = (await vscode.execFunction(
			"run_cmd",
			{
				cmd,
				page: page - 1,
				entries_num: perPage,
				saveHistory,
			},
			signal,
		)) as CasResultsEvent);
		const histEntry = history.current();
		if (
			!data.ERROR &&
			histEntry &&
			saveHistory &&
			!isEqual(histEntry, [command, page, histEntry[2]])
		) {
			history.push([command, page, perPage]);
		}
	} else {
		data = {
			ERROR: "No command provided",
			count: 0,
		};
	}
	resolve();
	waiting = false;
	if (!setState) {
		return data;
	}
	results = data;
	return results.ERROR === undefined;
}
function extractFilename(
	obj: Exclude<EntryArray, string[]>[number],
): string | undefined {
	if ("filename" in obj) {
		return obj.filename as string;
	}
	if ("path" in obj) {
		return obj.path as string;
	}
	if ("file" in obj) {
		return obj.file as string;
	}
	return undefined;
}

async function generateWorkspace(cmd: string, update = false) {
	let selections: string[] = [];
	let func = "gen_ws_cmd";
	if (selected.length) {
		selections =
			typeof selected[0] === "string"
				? (selected as string[])
				: ((selected as Exclude<EntryArray, string[]>)
						.map(extractFilename)
						.filter((v) => v !== undefined) as string[]);
		func = "selected_items";
	}
	if (update) {
		func = "update_ws_cmd";
	}
	await vscode.execFunction(func, {
		cmd,
		selections: $state.snapshot(selections),
	});
}
let historyPosition = -1;
$inspect(command);
async function traverseHistory(direction: "back" | "forward") {
	console.log(`tracerse:  ${direction}`);
	historyPosition += direction === "back" ? -1 : 1;
	command = history?.undo()?.[0] ?? command;
	({ data: results } = (await vscode.execFunction("hist", {
		action: direction,
	})) as CasResultsEvent);
}

const selectable = $derived(
	results?.ERROR === undefined &&
		(typeof results?.entries?.at(0) === "string" ||
			"filename" in ((results?.entries?.at(0) as object | undefined) ?? {})),
);
</script>

<main>
    <CasCommand
        bind:this={casCommand} 
        bind:perPage={perPage}
        bind:command={command}
		bind:waiting={waiting}
        {runCommand}
        nextCommand={() => traverseHistory("forward")}
        previousCommand={() => traverseHistory("back")}
        bind:history={history}
        class="mb-4"
    >
        {#snippet endButtons()}
            <vscode-button  class="grow text-nowrap -mr-2" onclick={() => generateWorkspace(command)}>
                <span class="text-nowrap w-fit">
                    Generate Workspace
                </span>
            </vscode-button>
            {#if isGenerated}
                <vscode-button  class="grow text-nowrap -mr-2" onclick={() => generateWorkspace(command, true)}>
                    <span class="text-nowrap w-fit">
                        Update Workspace
                    </span>
                </vscode-button>
            {/if}
        {/snippet}
    </CasCommand>
	{#if enabled && command && results}
		
		<CasResults
			commandError={results.ERROR}
			firstResult={results && "entries" in results ? results.entries.at(0) : undefined}
			getPage={async (newPage, newPerPage, sort) => {
				page = newPage;
				perPage = newPerPage;
				return runCommand(command + (sort ? ` --sorted --sorting-key=${sort?.colId}${sort?.sort === "asc" ? "" : " --reverse"}` : ""), undefined, false, false) as Promise<CASResults>;
			}}
			count={results && "count" in results ? results.count : 0}
			allowSelection={selectable}
			bind:selected={selected}
			bind:perPage={perPage}
		/>
	{:else if enabled}
		<div class="flex flex-col w-full items-center">
			
		</div>
	{:else}
		<div class="flex flex-col w-full items-center">
			<h1 class="block text-vscode-editorError-foreground w-max text-2xl">CAS not configured</h1>
			<p class="block w-max"><a href="command:cas.db.pick.bas" class="text-vscode-textLink-activeForeground">Configure the server manually</a> or use a workspace file</p>
		</div>
	{/if}
</main>