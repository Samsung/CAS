<script lang="ts">
import { overwriteGetLocale } from "$lib/paraglide/runtime.js";
import { telemetryInit, trackPageview } from "$lib/telemetry";
import "../app.css";

import { browser } from "$app/environment";
import { vscode } from "$lib/vscode";
import { onMount } from "svelte";
import "@vscode-elements/elements/dist/vscode-button";
import "@vscode-elements/elements/dist/vscode-table";
import "@vscode-elements/elements/dist/vscode-table-body";
import "@vscode-elements/elements/dist/vscode-table-row";
import "@vscode-elements/elements/dist/vscode-table-cell";
import "@vscode-elements/elements/dist/vscode-table-header";
import "@vscode-elements/elements/dist/vscode-table-header-cell";
import "@vscode-elements/elements/dist/vscode-icon";
import "@vscode-elements/elements/dist/vscode-badge";
import "@vscode-elements/elements/dist/vscode-collapsible";
import "@vscode-elements/elements/dist/vscode-progress-ring";
import "@vscode-elements/elements/dist/vscode-textfield";
import "@vscode-elements/elements/dist/vscode-single-select";
import "@vscode-elements/elements/dist/vscode-multi-select";
import "@vscode-elements/elements/dist/vscode-option";
import "@vscode-elements/elements/dist/vscode-radio";
import "@vscode-elements/elements/dist/vscode-radio-group";
import "@vscode-elements/elements/dist/vscode-checkbox";

interface AppWindow extends Window {
	page?: string;
}

if (browser) {
	onMount(async () => {
		await telemetryInit();
		trackPageview({
			url: `/webview/${(window as AppWindow).page}`,
		});
	});
	vscode.getState();
}
let { children } = $props();
</script>


<svelte:boundary>
	{@render children()}
</svelte:boundary>
