<script lang="ts">
import * as t from "$lib/paraglide/messages.js";
import { type CasApiEvent, vscode } from "$lib/vscode";
import { onMount } from "svelte";

let isGenerated = $state(false);
let snippets = $state<[string, string][]>([]);
interface InitEvent extends CasApiEvent {
	data: {
		isGenerated: boolean;
		snippets: [string, string][];
	};
}

onMount(async () => {
	({
		data: { isGenerated, snippets },
	} = (await vscode.execFunction("init", {})) as InitEvent);
	vscode.addEventListener(
		"snippets",
		(event: MessageEvent<{ data: { snippets: [string, string][] } }>) =>
			(snippets = event.data.data.snippets),
	);
});
</script>

<fieldset class="border border-vscode-panel-border rounded-md flex flex-col w-full my-2 test">
    <legend class="font-bold">{t.databaseTools()}</legend>
    <fieldset class="grid grid-rows-1 grid-cols-2 gap-1 m-2 justify-center">
        <vscode-button onclick={() => vscode.execFunction('opn_cmd_win', {})}>{t.databaseClient()}</vscode-button>

        <vscode-button onclick={() => vscode.execFunction('opn_ptree_win', {})}>{t.processTree()}</vscode-button>

        <vscode-button onclick={() => vscode.execFunction('opn_flamegraph', {})}>FlameGraph</vscode-button>

    </fieldset>
    {#if isGenerated}
        <fieldset class="grid grid-rows-1 grid-cols-2 gap-1 mx-2 mb-2 mt-0.5 justify-center">
            <vscode-button onclick={() => vscode.execFunction('update_ws', {})}>{t.updateWorkspace()}</vscode-button>
            <vscode-button onclick={() => vscode.execFunction('edit_ws', {})}>{t.editWorkspace()}</vscode-button>
        </fieldset>
    {/if}
</fieldset>
<fieldset class="border border-vscode-panel-border rounded-md flex flex-col w-full my-2">
    <legend class="font-bold">{t.codeTools()}</legend>
    <fieldset class="grid grid-rows-1 grid-cols-2 gap-1 m-2 justify-center">
        <vscode-button onclick={() => vscode.execFunction('opn_og_win', {})}>{t.og()}</vscode-button>
    </fieldset>
</fieldset>
{#if snippets?.length}
    <fieldset class="border border-vscode-panel-border rounded-md flex flex-col w-full">
        <legeld class="font-bold -mt-2.5">{t.shortcuts()}</legeld>
        <fieldset class="w-full snippet grid grid-cols-2 lg:grid-cols-4 justify-start gap-4 p-2">
        {#each snippets as snippet}

                <vscode-button 
                data-vscode-context={JSON.stringify({name: snippet[0], elem_type: "snippet"})}

                    onclick={() => vscode.execFunction("run_cmd", {data: snippet[1]})}
                >
                    {snippet[0] in t ? t[snippet[0] as keyof typeof t] : snippet[0] }
                </vscode-button>
                {/each}
            </fieldset>
    </fieldset>
{/if}