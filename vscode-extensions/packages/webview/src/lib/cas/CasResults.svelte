<script module lang="ts">
export type ElementTypes =
	| ""
	| "cmd"
	| "compiled"
	| "linked"
	| "bin"
	| "path"
	| "pid";
</script>
<script lang="ts">
import type { HTMLAttributes } from "svelte/elements";
import type { CASResults, EntryArray, entry } from "./types";
import { isEqual } from "lodash-es"
import { AgGrid, makeSvelteCellRenderer } from "@opliko/ag-grid-svelte5-extended";
import { RowSelectionModule, type ColDef,
	type GridOptions,
	type ColTypeDef,
    type SortModelItem,
    type IGetRowsParams,
    type GridApi, themeQuartz, InfiniteRowModelModule,
    ColumnAutoSizeModule,
    PaginationModule,
    RowAutoHeightModule,
    CsvExportModule,
    AllCommunityModule,
} from "ag-grid-community"
import { copyToClipboard } from "@svelte-put/copy";
import CasResultCell from "./CasResultCell.svelte";
import SelectAllCell from "./SelectAllCell.svelte";

interface Props extends HTMLAttributes<HTMLElement> {
	commandError?: string
	firstResult?: entry;
	getPage: (page: number, perPage: number, sort?: SortModelItem) => Promise<CASResults>;
	allowSelection?: boolean;
	selected?: EntryArray;
	count: number;
	perPage: number;
}

let {
	commandError,
	firstResult,
	getPage,
	allowSelection = true,
	selected = $bindable([]),
	count,
	perPage = $bindable(50),
	...props
}: Props = $props();

$effect(() => {
	api?.setGridOption?.("paginationPageSize", perPage);
})
let lastFirstResult = firstResult;
$effect(() => {
	if (api && !isEqual($state.snapshot(firstResult), lastFirstResult)) {
		lastFirstResult = firstResult;
		api?.purgeInfiniteCache()
	}
})

let error = $state<string | undefined>(undefined);

$effect(() => {
	if (commandError) {
		error = commandError
	}
})

const cellTypes: Record<string, ColTypeDef> = {
	pid: { cellDataType: "number" },
	idx: { cellDataType: "number" },
	ppid: { cellDataType: "number" },
	pidx: { cellDataType: "number" },
	path: { cellDataType: "string" },
	source_type: { cellDataType: "string", maxWidth: 200 },
	command: { cellDataType: "object", autoHeight: true, wrapText: true },
	compiled_files: { cellDataType: "array", autoHeight: true, wrapText: true},
	include_paths: { cellDataType: "array", autoHeight: true, wrapText: true},
	defs: { cellDataType: "array", autoHeight: true, wrapText: true},
	undefs: { cellDataType: "array", autoHeight: true, wrapText: true},
	headers: { cellDataType: "array", autoHeight: true, wrapText: true},
	deps: { cellDataType: "array", autoHeight: true, wrapText: true},
};

const sortableKeys = [
	"bin",
	"cwd",
	"cmd",
	"path",
	"original_path",
	"mode"
]

function getColumnDefs(firstResult?: entry): ColDef[] {
	if (!firstResult) {
		return [];
	}
	if (typeof firstResult === "string") {
		return [
			{
				colId: "path",
				field: "path",
				type: "path",
				flex: 1,
				context: {
					elem_type: "path"
				}
			},
		];
	}
	return Object.keys(firstResult).map((key) => ({
		field: key,
		colId: key.toLowerCase(),
		type: cellTypes[key.toLowerCase()] ? key.toLowerCase() : undefined,
		context: { elem_type: mapToElemType(key) },
		sortable: sortableKeys.includes(key)
	}));
}

const columnDefs = getColumnDefs(firstResult);

function mapToElemType(key: string): ElementTypes {
	switch (key) {
		case "pid":
		case "idx":
		case "ppid":
		case "pidx":
			return "pid";
		case "path":
		case "openfiles":
		case "file":
		case "filename":
		case "original_path":
			return "path";
		case "bin":
			return "bin";
		case "linked":
			return "linked";
		case "compiled":
			return "compiled";
		case "command":
			return "cmd";
		default:
			return "";
	}
}

function normalizeEntries(entries: EntryArray, sortSelected?: "asc" | "desc") {
	if (typeof entries.at(0) === "string") {
		if (sortSelected && typeof selected.at(0) === typeof entries.at(0)) {
			entries = entries.filter(entry => !selected.find(selectedEntry => typeof selectedEntry === typeof entry ? typeof selectedEntry === "string" ? selectedEntry === entry : JSON.stringify(entry) === JSON.stringify(selectedEntry) : false)) as EntryArray;
			sortSelected === "asc" ? entries.unshift(...($state.snapshot(selected) as any)) : entries.push(...($state.snapshot(selected) as any))
		}
		return entries.map((entry) => ({ path: entry }));
	}
	return entries;
}


function getRows({ context, startRow, endRow, successCallback, failCallback, sortModel }: IGetRowsParams) {
	api?.setGridOption("loading", true);
	getPage(1 + startRow / (endRow - startRow), endRow - startRow, sortModel.at(0)?.colId !== "selected" ? sortModel.at(0) : undefined )
		.then((results) => {
			if (!results || results.ERROR || !("entries" in results)) {
				if (results.ERROR) {
					error = results.ERROR;
				}
				throw new Error(results.ERROR);
			}
			error = undefined;
			api!.updateGridOptions({ 
				loading: false,
				columnDefs: getColumnDefs(results.entries.at(0)),
				rowSelection: allowSelection ? {
					mode: "multiRow",
					headerCheckbox: false
				} : {
					mode: "singleRow"
				},
			});
			return successCallback(
				normalizeEntries(results.entries, sortModel.at(0)?.colId === "selected" ? sortModel.at(0)?.sort : undefined),
				results.count,
			);
		})
		.catch((e) => {
			api?.setGridOption("loading", false);
			error = e;
			console.error(e)
			failCallback();
		});
}

let allSelected = $state(false);
const theme = themeQuartz.withParams({
		accentColor: "var(--vscode-focusBorder)",
		backgroundColor: "var(--vscode-panel-background)",
		oddRowBackgroundColor: "var(--vscode-tree-tableOddRowsBackground)",
		borderColor: "var(--vscode-tree-tableColumnsBorder)",
		borderRadius: 0,
		browserColorScheme: "dark",
		chromeBackgroundColor: "var(--vscode-keybindingTable-headerBackground)",
		fontFamily: "inherit",
		foregroundColor: "var(--vscode-foreground)",
		headerBackgroundColor: "var(--vscode-keybindingTable-headerBackground)",
		headerFontSize: 14,
		wrapperBorderRadius: 0,
	});
let api = $state<GridApi<entry>>()
let gridOptions: GridOptions<entry> = {
	defaultColDef: {
		suppressMovable: true,
		cellRenderer: makeSvelteCellRenderer(CasResultCell, "cell")
	},
	columnDefs: columnDefs,
	columnTypes: cellTypes,
	rowSelection: allowSelection ? {
			mode: "multiRow",
		} : {mode: "singleRow"},
	onSelectionChanged({api}) {
		selected = api.getSelectedRows() as EntryArray;
	},
	selectionColumnDef: {
		maxWidth: 50,
		flex: 0,
		headerComponent: makeSvelteCellRenderer(SelectAllCell),
		headerComponentParams: {
			selectAll: () => {
				const nodes = api?.getRenderedNodes();
				allSelected = nodes?.every(node => node.isSelected()) ?? false;
				for (const node of nodes ?? []) {
					node.setSelected(!allSelected)
				}
				return !allSelected;
			},
		},
		cellClass: "w-min"
	},
	// domLayout: "autoHeight",
	rowModelType: "infinite",
	infiniteInitialRowCount: Math.max(Math.min(count, perPage), 1),
	paginationPageSize: perPage,
	paginationPageSizeSelector: [10, 50, 100, 1000],
	paginationAutoPageSize: false,
	onPaginationChanged({newPageSize, api}) {
		if (newPageSize && perPage !== api.paginationGetPageSize()) {
			perPage = api.paginationGetPageSize()
		}
	},
	datasource: {
		getRows: getRows
	},
	getRowId({ data }) {
		if (typeof data !== "object") {
			return data;
		}
		if ("filename" in data) {
			return data.filename;
		}
		if ("pid" in data) {
			return data.pid + ("idx" in data ? `:${data.idx}` : "");
		}
		if ("command" in data) {
			return `${data.command.pid}:${data.command.idx}`;
		}
		return Object.values(data).join(";");
	},
	theme,
	pagination: true,
	copyHeadersToClipboard: true,
	rowBuffer: perPage,
	onCellClicked(event) {
		try {
			copyToClipboard(event.value);
		} catch (_e) {
			
		}
	},
	onGridReady(event) {
		api = event.api;
	},	
};

const modules = [InfiniteRowModelModule, RowSelectionModule, ColumnAutoSizeModule, PaginationModule, RowAutoHeightModule, CsvExportModule, AllCommunityModule];
</script>

{#if firstResult === undefined || error}
	<div class="flex flex-col w-full items-center">
		<h1 class="block text-vscode-editorError-foreground w-max text-2xl">{ error ? "Error" : "No Results"}</h1>
		{#if error}
			<p class="w-full">{error}</p>
		{/if}
	</div>
{:else}
<div class="min-h-min h-[90vh]">
	<AgGrid gridOptions={gridOptions} {modules} sizeColumnsToFit={true} {theme} gridClass="resultGrid" ></AgGrid>    
</div>

{/if}

<style>
	:global(.resultGrid) {
		min-height: 75vh;
		height: 75vh;
	}
</style>