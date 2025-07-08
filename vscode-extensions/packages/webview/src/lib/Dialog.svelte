<script lang="ts">
import type { Snippet } from "svelte";

// TODO: type this properly
interface Props {
	children: Snippet;
	modal: any;
}
let { children, modal, ...rest }: Props = $props();

function show(node: any) {
	if (modal) {
		node.showModal();
	}
}
</script>

<dialog use:show {...rest}>
	{@render children()}	
</dialog>
