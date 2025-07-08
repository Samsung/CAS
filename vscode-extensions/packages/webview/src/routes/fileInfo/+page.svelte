<script lang="ts">
import { type CasApiEvent, vscode } from "$lib/vscode";
import type { FileMode, FileType } from "@cas/types/bas.js";
import { onMount } from "svelte";

type FileData = {
	filename: string;
	ppid: number;
	type: FileType;
	access: FileMode;
	link: 0 | 1;
};
interface InitEvent extends CasApiEvent {
	data: {
		fileData: FileData;
	};
}
let fileData = $state<FileData | undefined>(undefined);
onMount(async () => {
	({
		data: { fileData },
	} = (await vscode.execFunction("init", {})) as InitEvent);
	vscode.addEventListener(
		"init",
		(event: MessageEvent<{ data: { fileData: FileData } }>) =>
			(fileData = event.data.data.fileData),
	);
});
</script>
    
<vscode-table bordered-columns zebra>
    <vscode-table-body slot="body">
        {#each Object.entries(fileData ?? {}) as [name, value]}
        <vscode-table-row class="grid" style="grid-template-columns: 5rem 1fr">
            <vscode-table-cell>{name}</vscode-table-cell>
            <vscode-table-cell>{value}</vscode-table-cell>
        </vscode-table-row>
        {/each}
    </vscode-table-body>
</vscode-table>
