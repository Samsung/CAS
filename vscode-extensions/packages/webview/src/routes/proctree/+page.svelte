<svelte:options runes={true} />
<script lang="ts">
import { sleep, withAbort } from "@cas/helpers/promise.js";
import type { Process } from "@cas/types/bas.js";
import { copy } from "@svelte-put/copy";
import { join as shjoin } from "shlex";
import { onMount, tick } from "svelte";
import ProcessModal from "$lib/cas/ProcessModal.svelte";
import ProcSearch, {
	queryToSearch,
	type SearchQuery,
} from "$lib/cas/ProcSearch.svelte";
import {
	chunk,
	eidEq,
	eidToStr,
	nsFormat,
	stopPropagation,
} from "$lib/helpers";
import * as t from "$lib/paraglide/messages.js";
import Tree from "$lib/treeview/Tree.svelte";
import type { TreeType } from "$lib/treeview/types";
import type {
	eid,
	InitEventData,
	ProcessEventData,
	ProcNode,
	SearchResults,
	SetChildrenEventData,
} from "$lib/types";
import { vscode } from "$lib/vscode";
import VsCodeTooltipButton from "$lib/vscode/VSCodeTooltipButton.svelte";

let initialized = $state<false | true | "failed">(false);
onMount(async () => {
	const initialize = async (pidList: eid[]) => {
		tree = [];
		await Promise.all(
			pidList.map((eid) =>
				getProcess(eid).then((value) => value && (initialized = true)),
			),
		);
	};
	let i = 0;
	while (!initialized) {
		const {
			data: { pid_list, server_url },
		} = (await withAbort(
			vscode.execFunction("init", {}),
			// simple exponential timeout in case server is just slow
			AbortSignal.timeout(250 * 2 ** i++),
		).catch(async () => {
			initialized = "failed";
			// exponential backoff to not spam the server
			await sleep(25 * 2 ** i);
			return { data: { pid_list: [], server_url: undefined } };
		})) as InitEventData;
		await initialize(pid_list);
	}
	vscode.addEventListener("init", (event) => {
		initialize((event.data as InitEventData).data.pid_list);
	});
});
async function getProcess(eid: eid, insert: boolean = true): Promise<ProcNode> {
	const { data: process } = (await vscode.execFunction("getProcess", {
		eid,
	})) as ProcessEventData;
	return setProcessData(process, insert);
}

function findNode(
	searchedTree: TreeType<ProcNode>,
	eid: eid,
	peid?: eid,
	parent?: ProcNode,
): { node: ProcNode | null; parent: ProcNode | null } {
	for (const node of searchedTree) {
		if (eidEq(node.eid, eid)) {
			return { node, parent: parent ?? null };
		}
		if (node?.children?.length > 0) {
			const { node: foundNode, parent: foundParent } = findNode(
				node.children,
				eid,
				peid,
				parent,
			);
			if (foundNode || foundParent) {
				return { node: foundNode, parent: foundParent };
			}
		}
		if (eidEq(node.eid, peid)) {
			return { node: null, parent: node };
		}
	}
	return { node: null, parent: parent ?? null };
}

function setProcessData(data: Process, insert = true, knownParent?: ProcNode) {
	const { pid, idx, children, ppid, pidx } = data;
	const eid = { pid, idx };
	const peid = { pid: ppid, idx: pidx };

	const { node, parent } = knownParent
		? {
				node: knownParent.children.find((n) => eidEq(eid, n.eid)),
				parent: knownParent,
			}
		: findNode(tree, eid, peid);

	if (node) {
		node.data = {
			...(node?.data ?? {}),
			...data,
		};
		if (parent && !node.parent) {
			node.hasParent = true;
			node.parent = parent;
			parent.children.push(node);
			const index = tree.findIndex(
				(n) => n.data.pid === pid && n.data.idx === idx,
			);
			if (index >= 0) {
				tree.splice(index, 1);
			}
		}
		return node;
	}
	const nodeData: ProcNode = {
		eid,
		children: [],
		hasParent: !!parent,
		parent,
		expanded: false,
		hasChildren: children > 0,
		data,
	};
	if (parent) {
		parent.children.push(nodeData);
	} else if (insert) {
		tree = [...tree, nodeData];
	}
	return nodeData;
}
async function getParent(node: ProcNode) {
	const peid = { pid: node.data.ppid, idx: node.data.pidx };
	const { node: parentNode } = findNode(tree, peid);
	const parent = parentNode ?? (await getProcess(peid, false));
	if (parent) {
		parent.expanded = true;
		parent.children.push(node);
		node.hasParent = true;
		const index = tree.findIndex((v) => eidToStr(v.eid) === eidToStr(node.eid));
		if (parent.hasParent) {
			let grandparent = parent.parent;
			while (grandparent) {
				grandparent.expanded = true;
				grandparent = grandparent.parent;
			}
			tree.splice(index, 1);
		} else {
			tree[index] = parent;
		}
		for (const node of tree.filter((node) =>
			eidEq({ pid: node.data.ppid, idx: node.data.pidx }, parent.eid),
		)) {
			const index = tree.findIndex((n) => eidEq(n.eid, node.eid));
			tree.splice(index, 1);
			if (parent.children.find((n) => eidEq(node.eid, n.eid))) {
				continue;
			}
			node.parent = parent;
			node.hasParent = !!parent;
			parent.children.push(node);
		}
	}
}

let waiting = $state(false);

async function search(
	query: SearchQuery,
	signal: AbortSignal,
): Promise<Process[]> {
	if (Object.values(query).every((val) => val === undefined)) {
		tree = [];
		vscode.postMessage({ func: "init" });
		return [];
	}
	const {
		data: { execs, count },
	} = (await vscode.execFunction("search", {
		data: queryToSearch(query).toString(),
	})) as SearchResults;
	const results = count && execs ? execs : [];
	let cleared = false;
	// load two pages and then 16 pages at a time
	// this techinically slows down loading, but allows the page to remain responsive
	// that's also why we initially load less items - since first pages result in more DOM updates
	const initialPageSize = perPage * 2;
	const laterPageSize = perPage * 16;
	for (const page of chunk(results, laterPageSize, initialPageSize)) {
		const foundNodes: ProcNode[] = [];
		for (const node of page) {
			if (signal.aborted) {
				return results;
			}
			const eid = { pid: node.pid, idx: node.idx };
			foundNodes.push({
				eid,
				children: [],
				expanded: false,
				hasChildren: node.children > 0,
				hasParent: false,
				parent: null,
				data: node,
			});
		}
		if (results.length > initialPageSize) {
			if (!cleared) {
				tree = foundNodes;
				cleared = true;
				await sleep(0);
			} else {
				tree.push(...foundNodes);
			}
			await sleep(0);
			await tick();
		} else {
			tree = foundNodes;
		}
	}

	return results;
}

async function getChildData(node: ProcNode, page = 0) {
	if (node.children.length >= node.data.children) {
		return;
	}
	const eid = { pid: node.data.pid, idx: node.data.idx };
	const {
		data: { pages, children },
	} = (await vscode.execFunction("getChildren", {
		eid,
		data: { page },
	})) as SetChildrenEventData;
	for (const child of children) {
		setProcessData(child, false, node);
	}
	if (pages && page === 0) {
		await Promise.all(
			Array.from({ length: pages - 1 }).map((_, i) =>
				getChildData(node, i + 1),
			),
		);
	}
}

let tree: TreeType<ProcNode> = $state([]);
function hasInfo(node: ProcNode) {
	return node?.data?.cwd || node?.data?.bin || node?.data?.cmd?.length;
}
function hasChildren(node: ProcNode) {
	return node?.data?.children > 0;
}

const perPage = 20;

let filterEmpty = $state(false);
function filterFunc(node: ProcNode) {
	// every node passes through this function even if the filter isn't enabled
	// so we short-circuit to visible if the filter is disabled
	if (!filterEmpty) {
		return true;
	}
	// current criteria for empty ndoes: no children, opens or command
	// TODO: make this configurable somehow?
	if (node.hasChildren || node.data.open_len > 0 || node.data.bin.length > 0) {
		return true;
	}
	return false;
}

let etimeSort = $state(false);
function sortByEtime(nodes: ProcNode[]): void {
	// recursively sorts the tree in place
	nodes.sort((a, b) => a.data.etime - b.data.etime);
	for (const node of nodes) {
		if (!node.hasChildren) {
			continue;
		}
		sortByEtime(node.children);
	}
}
// run whenever the tree changes if sorting is enabled
$effect(() => {
	if (etimeSort) {
		sortByEtime(tree);
	}
});
// reset the tree by just setting it to the selected node and removing its parent connection
// hopefully GC should handle the now-disconnected tree
function setRoot(node: ProcNode) {
	node.hasParent = false;
	delete node.parent;
	tree = [node];
}
let cellExpanded = $state<Record<string, boolean>>({});
</script>

<main>
	<ProcSearch
		bind:filter={filterEmpty}
		bind:etime_sort={etimeSort}
		search={search}
	/>
	{#if tree.length}
	    <div class="bg-vscode-panel-background max-w-screen">
			<Tree
				bind:tree={tree}
				loadChildren={getChildData}
				{filterFunc}
				{perPage}
			>
			{#snippet children(node, toggle, expanded)}
				<!-- definition of each "node" in the tree -->
				<div class="flex flex-row">

					{#if node?.data?.ppid && !node.hasParent }
							<vscode-icon
								onclick={stopPropagation(() => getParent(node))}
								name="debug-step-back"
								actionIcon={true}
							>
							</vscode-icon>
					{/if}
					<vscode-table 
						data-ppid={node.data.ppid}
						data-pidx={node.data.pidx}
						columns={["200px", "32px", "100px", "100px", "32px", "auto"]}
					>
						<vscode-table-body slot="body">
							<vscode-table-row class="flex flex-row shrink grow justify-start p-0 max-w-full">
								<vscode-table-cell class="cellId p-0 min-w-max" >
									<vscode-button 
									onclick={stopPropagation(async () => {
										if (node.expanded && (node?.children?.length ?? 0) < node.data.children) {
											getChildData(node)
										} else {
											toggle();
										}
									})}
											appearance={node.data?.children > 0 ? "primary" : "secondary"}
											aria-label={`expand ${node?.eid}`}
											class="min-w-max flex flex-row align-middle justify-start items-center"
											>
												{#if expanded && node.children.length < node.data.children}
												<vscode-icon name="debug-restart"></vscode-icon>
												{:else if !expanded && hasChildren(node)}
												<vscode-icon name="chevron-right"></vscode-icon>

												{:else if hasChildren(node)}
												<vscode-icon name="chevron-down"></vscode-icon>

												{:else}
												<vscode-icon name="dash"></vscode-icon>

												{/if}
											<span class="min-w-max h-min">
												{node?.data?.pid}:{node?.data?.idx}
											</span>
											<vscode-badge variant="counter" class="children-badge">{node?.data?.children}</vscode-badge>
										</vscode-button>
									</vscode-table-cell>
									{#if hasInfo(node)}
										<vscode-table-cell
										class="py-0 pr-1 pl-2 self-center flex justify-center min-w-8"
										
										onclick={stopPropagation()}
									>
										<ProcessModal {node} />
									</vscode-table-cell>
								{/if}
								{#if node?.data?.etime !== undefined && node.data.etime > 0}
									{@const formatted = nsFormat(node.data.etime)}
									{#if formatted}
										<vscode-table-cell class="cellTime mx-1 px-0 min-w-max"  use:copy={{text: formatted}} onclick={stopPropagation()}>
											<vscode-badge class="tag">
												{formatted}
											</vscode-badge>
										</vscode-table-cell>
									{/if}
								{/if}
								<vscode-table-cell class="openCount mx-1 px-0 min-w-max"  use:copy={{text: node?.data?.open_len.toFixed(0) ?? "0"}} onclick={stopPropagation()}>
									<vscode-badge class="tag">
										{t.nOpenedFiles({n: node?.data?.open_len ?? 0})}
									</vscode-badge>
								</vscode-table-cell>
								<vscode-table-cell class="px-1 pt-0.5 pb-0 m-0" onclick={stopPropagation()}>
									<VsCodeTooltipButton
									description="Start from this process"

									onclick={() => setRoot(node)}
									icon="symbol-class"
									>

									</VsCodeTooltipButton>
								</vscode-table-cell>
								<vscode-table-cell class="w-max max-w-full text-nowrap whitespace-nowrap overflow-hidden text-ellipsis h-auto"  use:copy class:text-pretty={cellExpanded[eidToStr(node.eid)]} onclick={() => window.getSelection()?.type !== "Range" ? cellExpanded[eidToStr(node.eid)] = !cellExpanded[eidToStr(node.eid)] : undefined}>
									{shjoin([node?.data?.bin, ...(node?.data?.cmd.slice(1) ?? [])].filter(Boolean))}
								</vscode-table-cell>
							</vscode-table-row>
						</vscode-table-body>
					</vscode-table>
				</div>

				{/snippet}
				{#snippet placeholder(node: ProcNode)}
					<div class={`h-7 min-h-7`}></div>
					{#each node.children as child}
						<div class={`h-7 min-h-7`}></div>
					{/each}
				{/snippet}
			</Tree>
    	</div>
	{:else if initialized === "failed"}
		<div class="flex flex-col w-full items-center">
			<h1 class="block text-vscode-editorError-foreground w-max text-2xl">CAS not configured</h1>
			<p class="block w-max"><a href="command:cas.db.pick.bas" class="text-vscode-textLink-activeForeground">Configure the server manually</a> or use a workspace file</p>
		</div>
		{/if}
    {#if waiting}
      <div class="flex grow align-middle justify-center">
        <vscode-progress-ring></vscode-progress-ring>
      </div>
    {/if}
</main>

<style>
  main {
    display: flex;
    flex-direction: column;
  }
  .grid {
    background: var(--vscode-panel-background);
  }
  .row {
    display: flex;
    align-items: start;
  }
  .children-badge {
    margin-left: 0.25rem;
  }
  .tag::part(control) {
    text-transform: none;
  }
  .tag {
    min-width: max-content;
  }

  .command {
    text-overflow: ellipsis;
    overflow: hidden;
    white-space: nowrap;
    padding-right: 0.25rem;
  }
  .top-grid {
    grid-template-columns: 1fr max-content;
  }
</style>