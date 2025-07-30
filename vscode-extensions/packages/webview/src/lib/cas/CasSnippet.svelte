<script lang="ts">
import { createDialog, melt } from "@melt-ui/svelte";
import { vscode } from "$lib/vscode";
import VsCodeTooltipButton from "$lib/vscode/VSCodeTooltipButton.svelte";

const { command } = $props<{ command: string }>();
const {
	elements: { trigger, overlay, content, title, description, close, portalled },
	states: { open },
} = createDialog({
	forceVisible: true,
});

let name = $state("");
let savedCommand = $state(command);
$effect(() => {
	console.log(`command change ${!command.length}`);
	savedCommand = command;
	open.set(false);
});
</script>
<VsCodeTooltipButton
          description="Save as a snippet"
          placement="bottom"
          onclick={(event) => $trigger.action(event.currentTarget as HTMLElement)}
          disabled={!command.length || undefined}
          icon="save"
        >
</VsCodeTooltipButton>

{#if $open}
<div use:melt={$portalled}>
    <div use:melt={$overlay} class="fixed inset-0 z-50 bg-black/50"></div>
    <div
      use:melt={$content}
      class="fixed left-1/2 top-1/2 z-50 max-h-64 w-min
            -translate-x-1/2 -translate-y-1/2 rounded-xl
            bg-vscode-panel-background border-vscode-panel-border
            p-6 shadow-lg
            overflow-y-auto
            modal
            "
    >
        <h2 use:melt={$title} class="font-extrabold text-lg">
            Save as a snippet
        </h2>
        <div use:melt={$description}>
            <fieldset class="flex flex-col grow">
                <vscode-label for="name" required>
                    Snippet name:
                </vscode-label>  
                <vscode-textfield name="name" oninput={(event: InputEvent) => name=(event.currentTarget as HTMLInputElement).value  } value={name}>
                    Snippet name
                </vscode-textfield>
                <vscode-label for="name" required>
                    Snippet command:
                </vscode-label>
                <vscode-textfield oninput={(event: InputEvent) => savedCommand=(event.currentTarget as HTMLInputElement).value} value={savedCommand}>
                    Command
                </vscode-textfield>
            </fieldset>
        </div>
        <div class="flex flex-row justify-between mt-4">
            <vscode-button  use:melt={$close} onclick={() => vscode.execFunction("create_snippet", { name, value: savedCommand })}>Save</vscode-button>
        </div>
    </div>
</div>
{/if}