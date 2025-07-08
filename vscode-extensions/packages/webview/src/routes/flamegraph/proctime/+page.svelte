<script lang="ts">
import ProcessBottomTab from "$lib/cas/ProcessBottomTab.svelte";
import FlameGraph from "$lib/FlameGraph.svelte";
import { eidToStr, procHasChildren } from "$lib/helpers";
import ProcTimeGraph from "$lib/ProcTimeGraph.svelte";
import { vscode } from "$lib/vscode";
import { observeCssVar } from "$lib/vscode/themeObserver.svelte";
import type { ProcessWithChildren } from "@cas/types/bas.js";
import chroma, { type Color } from "chroma-js";
import { onMount, tick } from "svelte";

let proc = $state<ProcessWithChildren>();
let source_root = $state<string>();
let threads = $state<{ threads: number[]; count: number }>();
onMount(async () => {
	({
		data: { proc, source_root, threads },
	} = (await vscode.execFunction("init", {})) as {
		data: {
			proc: ProcessWithChildren;
			source_root: string | undefined;
			threads: { threads: number[]; count: number };
		};
	});
	console.log(proc);
	if (Array.isArray(proc.children)) {
		proc.children = filterEmpty(proc.children);
	}
});

/**
 * Remove entirely useless processes - ones that just are just used in spawning the actual process but don't do anything themselves
 * Common with e.g. bash processes, basically doubles the actual process and makes it harder to read
 *
 * @param procs
 */
function filterEmpty(procs: ProcessWithChildren[]) {
	const empty = new Map<string, ProcessWithChildren>();
	procs = procs.filter((proc) => {
		if (!proc.open_len && !procHasChildren(proc)) {
			empty.set(`${proc.pid}:${proc.idx}`, proc);
			return false;
		}
		return true;
	});
	for (const proc of procs) {
		empty.delete(`${proc.pid}:0`);
		if (Array.isArray(proc.children)) {
			proc.children = filterEmpty(proc.children);
		}
	}
	return procs.concat([...empty.values()]);
}
const nameFilter = $derived(new RegExp(`^.+/|${source_root}`));

function filterName(name: string) {
	return name.replace(nameFilter, "");
}

function getCmdName(cmd: string[]) {
	return filterName(cmd.join(" "))
		.replace(/\/bin\/(bash|sh) ?/, "")
		.split(" ")
		.at(0);
}

function getName(node: ProcessWithChildren) {
	return filterName(
		node.bin || getCmdName(node.cmd) || eidToStr(node.pid, node.idx),
	);
}
const variables = observeCssVar(
	"--vscode-charts-blue",
	"--vscode-charts-yellow",
	"--vscode-charts-green",
	"--vscode-charts-red",
).variables;

const baseColors = $derived({
	command: chroma(variables.get("--vscode-charts-red") ?? "#3794ff"),
	compiler: chroma(variables.get("--vscode-charts-green") ?? "#89d185"),
	linker: chroma(variables.get("--vscode-charts-blue") ?? "#cca700"),
	mix: chroma(variables.get("--vscode-charts-red") ?? "#f14c4c"),
});
function getBaseColor(node: ProcessWithChildren) {
	return baseColors[node.class].hex();
}
</script>

<div class="h-[calc(100vh-1rem)] min-h-full w-full -mx-1">
    {#if proc}
        <ProcTimeGraph 
		{proc}
		threads={threads?.threads ?? []}
		{getName}
		{getBaseColor}
		>
			{#snippet tooltip(node)}
				<h3>{node.bin || getCmdName(node.cmd)}</h3>
				<p>{node.pid}:${node.idx}</p>
				<p>{node.cmd.join(" ")}</p>
			{/snippet}
			{#snippet modal(node, open)}
				<ProcessBottomTab {node} {open} showButton={false} nameFilter={new RegExp(`^.+/|${source_root}`)}></ProcessBottomTab>
			{/snippet}
		</ProcTimeGraph>
    {/if}
</div>
    