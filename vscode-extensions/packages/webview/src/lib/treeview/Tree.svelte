<script lang="ts">
import { type Snippet } from "svelte";
import type { MaybeAsync } from "$lib/types.d.ts";
import type { ProcNode as NodeType } from "../types";
import Branch from "./Branch.svelte";
import type { Node, TreeType } from "./types";

interface Props {
	tree: TreeType<NodeType>;
	loadChildren?: ((node: NodeType) => MaybeAsync<void>) | undefined;
	filterFunc?: ((node: NodeType) => boolean) | undefined;
	class?: string | undefined;
	onexpand?: ((node: NodeType) => unknown) | undefined;
	oncollapse?: ((node: NodeType) => unknown) | undefined;
	children: Snippet<[NodeType, (expanded?: boolean) => unknown, boolean]>;
	placeholder?: Snippet<[NodeType]>;
	perPage?: number;
}
let {
	class: classes = "",
	tree = $bindable(),
	loadChildren = undefined,
	filterFunc = undefined,
	onexpand = undefined,
	oncollapse = undefined,
	children,
	placeholder,
	perPage,
}: Props = $props();

async function collapse(node: NodeType) {
	if (oncollapse) {
		oncollapse(node);
	}
}
async function expand(node: NodeType) {
	if (loadChildren) {
		loadChildren(node);
	}
	if (onexpand) {
		onexpand(node);
	}
}
</script>


<Branch
    bind:tree={tree}
    rootNode={null}
    depth={0}
    filterFunc={filterFunc}
    oncollapse={collapse}
    onexpand={expand}
    item={children}
    class={classes}
    perPage={perPage}
>
</Branch>