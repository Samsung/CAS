<svelte:options runes={true} />
<script lang="ts">
import { eidToStr } from "$lib/helpers";
import VsCodePaginator from "$lib/vscode/VSCodePaginator.svelte";
import { debounce } from "@melt-ui/svelte/internal/helpers";
import { onDestroy, onMount, type Snippet, tick } from "svelte";
// TODO: fix some generics issue
import type { ProcNode as NodeType } from "../types";
import Branch from "./Branch.svelte";
import type { Node, TreeType } from "./types";

interface Props {
	tree: TreeType<NodeType>;
	rootNode: NodeType | null;
	depth: number;
	filterFunc?: ((node: NodeType) => boolean) | undefined;
	class?: string;
	onexpand?: (node: NodeType) => unknown;
	oncollapse?: (node: NodeType) => unknown;
	observer?: IntersectionObserver;
	visibleNodes?: Set<string>;
	item: Snippet<[NodeType, (expanded?: boolean) => unknown, boolean]>;
	perPage?: number;
}
let {
	class: classes,
	tree = $bindable(),
	rootNode,
	depth,
	filterFunc,
	onexpand,
	oncollapse,
	item,
	perPage,
}: Props = $props();
function toggle(node: NodeType, expanded?: boolean) {
	if (expanded === undefined || typeof expanded !== "boolean") {
		expanded = !node.expanded;
	}
	if (expanded && onexpand) {
		onexpand(node);
	}
	if (!expanded && oncollapse) {
		oncollapse(node);
	}
	node.expanded = expanded;
}
function getToggle(node: NodeType) {
	return debounce((expanded?: boolean) => toggle(node, expanded), 50);
}
let listElement: HTMLUListElement;
let lists: HTMLLIElement[] = $state([]);
const visible = true;

function handleKeyboard(e: KeyboardEvent, i: number) {
	e.stopPropagation();
	switch (e.key) {
		case "ArrowRight": {
			toggle(tree[i], true);
			lists[i].blur();
			(lists[i]?.firstChild?.firstChild as HTMLElement)?.focus();
			break;
		}
		case "ArrowLeft": {
			toggle(tree[i], false);
			break;
		}
		case "ArrowDown": {
			if (i === lists.length) {
				return;
			}
			lists[i].blur();
			lists[i + 1]?.focus();
			break;
		}
		case "ArrowUp": {
			if (i === 0) {
				return;
			}
			lists[i].blur();
			lists[i - 1]?.focus();
			break;
		}
	}
}
const filteredTree = $derived(
	tree.filter(filterFunc ? filterFunc : () => true),
);
let page = $state(1);
let pages = $derived(perPage ? Math.ceil(filteredTree.length / perPage) : 1);
const expandedNodes = $derived(
	filteredTree.filter((node) => node.hasChildren && node.expanded),
);
const treePage = $derived(
	expandedNodes.concat(
		filteredTree
			.filter((node) => !node.expanded || !node.hasChildren)
			.slice((page - 1) * (perPage ?? 0), page * (perPage ?? tree.length)),
	),
);
</script>

{#snippet paginator()}
    {#if perPage && pages > 1}
    <nav class="flex flex-row justify-start gap-3 align-middle z-10 bg-vscode-panel-background relative">
        <VsCodePaginator
            bind:page={page}
            count={pages*perPage}
			{perPage}
            ellipsis={true}
			showRange={true}
        ></VsCodePaginator>
    </nav>
    {/if}
{/snippet}

<ul class:first={depth===0} class={classes} bind:this={listElement} 

>
    {@render paginator()}
    {#each treePage as node, i (eidToStr(node.eid))}
        <li
            class:child={node.hasParent}
            class:hasChildren={node.hasChildren}
            class={classes}
            bind:this={lists[i]}
        >
            {#if visible}
                {@render item(node, getToggle(node), node.expanded)}
            {/if}
            {#if node?.children?.length && node.expanded && visible}
                <Branch
                    class={classes}
                    bind:tree={node.children}
                    rootNode={node}
                    depth={depth+1}
                    filterFunc={filterFunc}
                    onexpand={onexpand}
                    oncollapse={oncollapse}
                    item={item}
                    perPage={perPage}
                    >
                </Branch>
            {/if}
            </li>
    {/each}
    {@render paginator()}
</ul>
<style>
    ul, li {
        margin: 0;
        list-style: none;
        position: relative;
    }
    .first {
        padding-inline-start: 0;
    }
    ul {
        padding-left: 2rem;
    }
    ul::before {
        content: "";
        position: absolute;
        top: 0;
        bottom: 0;
        left: 2rem;
        border-left: solid var(--vscode-tree-inactiveIndentGuidesStroke) 1px;
    }
    ul:hover::before {
        border-left: solid var(--vscode-tree-indentGuidesStroke) 1px;
    }
</style>