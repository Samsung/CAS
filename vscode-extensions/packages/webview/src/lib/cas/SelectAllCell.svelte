<script lang="ts">
import type { ICellRendererParams } from "ag-grid-community";

interface SelectAllParams extends ICellRendererParams {
	selectAll?: () => boolean;
	checked?: boolean;
}
let {
	value,
	colDef,
	selectAll,
	checked = $bindable(false),
}: SelectAllParams = $props();
</script>
    
<vscode-checkbox onvsc-change={() => checked = selectAll?.() ?? false} {checked}>
    
</vscode-checkbox>