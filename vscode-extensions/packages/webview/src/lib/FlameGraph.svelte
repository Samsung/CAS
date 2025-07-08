<script lang="ts" generics="T extends ProcessLike">
import {
	batchProcess,
	type MaybePromise,
	sleep,
	withAbort,
} from "@cas/helpers/promise.js";
import chroma from "chroma-js";
import { clamp, debounce } from "lodash-es";
import * as pixi from "pixi.js";
import { type IScale, Viewport } from "pixi-viewport";
import { type Snippet, tick } from "svelte";
import { Application, Container, ParticleContainer, Text } from "svelte-pixi";
import { observeCssVar } from "./vscode/themeObserver.svelte";
import "pixi.js/unsafe-eval";
import { TimelineRulerPlugin } from "$lib/TimelineRuler.svelte";
import MousePopover from "./MousePopover.svelte";

interface ProcessLike {
	children: any;
	etime: number;
	stime?: number;
}

interface Props<T extends ProcessLike> {
	proc: T;
	start?: number;
	depth?: number;
	parentColor?: chroma.Color;
	loadProc?: (proc: T) => MaybePromise<T>;
	setRoot?: (proc: T) => unknown;
	getName: (proc: T) => string;
	getBaseColor: (proc: T) => string;
	tooltip: Snippet<[T]>;
	modal: Snippet<[T, boolean]>;
	childrenComparator?: (a: T, b: T) => number;
}

const {
	proc,
	start = 0,
	depth = 0,
	loadProc,
	setRoot,
	getName,
	tooltip,
	getBaseColor,
	modal,
	childrenComparator,
}: Props<T> = $props();

let setRootAbort = new AbortController();

function inView(x: number, w: number, bounds: pixi.Rectangle) {
	return x + w > bounds.x && x - w < bounds.width + bounds.x;
}

const variables = observeCssVar(
	"--vscode-panel-background",
	"--vscode-font-family",
	"--vscode-charts-foreground",
	"--vscode-editorRuler-foreground",
	"--vscode-charts-foreground",
).variables;

function getProcessColor(
	color: string,
	timeFraction: number,
	children: number,
	level: number,
	index: number,
): string {
	const base = chroma(color).oklch();

	const hueShift = (index % 10) * 36;
	const newHue = (base[2] + hueShift) % 360;

	const saturation = Math.min(base[1] + children * 0.05, 0.7);
	const lightness = Math.max(base[0] - timeFraction * 0.15, 0.2);
	const shadeAdjust = level % 2 === 0 ? 0.05 : -0.05;
	const adjustedLightness = Math.min(
		Math.max(lightness + shadeAdjust, 0.1),
		0.6,
	);

	return chroma.oklch(adjustedLightness, saturation, newHue).hex();
}

//#region container size
let w = $state<number>();
let h = $state<number>();
let container = $state<pixi.Container>();
let application = $state<pixi.Application>();
let viewport: Viewport | undefined = undefined;

const resize = debounce((prevScreenSize: number) => {
	if (!application || !h || !w || !viewport) {
		return;
	}
	application.renderer.resize(w, h);
	if (viewport.plugins.get("clamp-zoom")) {
		const minScale = w / viewport.worldWidth;
		(viewport.plugins.get("clamp-zoom")!.options.minScale as IScale).x =
			minScale;
		if (prevScreenSize) {
			tick().then(() => viewport!.fitWidth(prevScreenSize, true, false, false));
		}
	}
}, 100);

$effect(() => {
	if (w && h && viewport && application) {
		const prevScreenSize = viewport.screenWidthInWorldPixels;
		resize(prevScreenSize);
	}
});

//#endregion

const culler = new pixi.Culler();

async function updateLeafNodes() {
	if (!viewport || !container) {
		return;
	}
	const bounds = viewport.getVisibleBounds();
	culler.cull(container, viewport!.getVisibleBounds(), false);

	for await (const leafBatch of batchProcess(
		leafNodes,
		async (leaf) => {
			if (leaf.bar.destroyed) {
				leafNodes.splice(
					leafNodes.findIndex(
						(l) => leaf.level === l.level && leaf.offset === l.offset,
					),
					1,
				);
				return;
			}
			const leafBounds = leaf.bar.bounds;
			if (
				viewport &&
				loadProc &&
				inView(leafBounds.x, leafBounds.width, bounds) &&
				leafBounds.width * 4 > bounds.width
			) {
				try {
					leaf.proc = await withAbort(
						loadProc(leaf.proc),
						AbortSignal.any([AbortSignal.timeout(60000), setRootAbort.signal]),
					);
				} catch {
					return undefined;
				}
				leafNodes.splice(
					leafNodes.findIndex(
						(l) => leaf.level === l.level && leaf.offset === l.offset,
					),
					1,
				);
				return leaf;
			}
			return undefined;
		},
		3,
		true,
	)) {
		for (const leaf of leafBatch.filter((l) => l !== undefined)) {
			try {
				await addBars(leaf.proc, leaf.level, leaf.offset, leaf.width);
				leaf.bar.parent.destroy({ children: true });
			} catch {}
		}
		await sleep(10);
	}
}

// ensure accuraccy better than +/-0.25
const worldWidth = 2 ** 50;
const timelineColors = $derived({
	textColor: chroma(
		variables.get("--vscode-charts-foreground") ?? "#ffffff",
	).num(),
	backgroundColor: chroma(
		variables.get("--vscode-panel-background") ?? "#000000",
	).num(),
	lineColor: chroma(
		variables.get("--vscode-editorRuler-foreground") ?? "#666666",
	).num(),
});

async function init() {
	while (!application || !container || !w || !h) {
		await tick();
	}
	application.canvas.width = w;
	application.canvas.height = h;
	application.stage.setSize(w, h);

	viewport = new Viewport({
		worldWidth: worldWidth,
		worldHeight: 1000,
		events: application.renderer.events,
		disableOnContextMenu: true,
		screenWidth: w,
		screenHeight: h,
	})
		.drag()
		.pinch()
		.wheel({ axis: "x" })
		.clamp({
			direction: "all",
			underflow: "top",
		})
		.clampZoom({
			minScale: {
				x: w / worldWidth,
				y: null,
			},
		});

	viewport.plugins.add(
		"timeline-ruler",
		new TimelineRulerPlugin(
			viewport,
			viewport,
			proc.stime ?? 0,
			(proc.stime ?? 0) + proc.etime,
			barHeight,
			timelineColors,
		),
	);

	await addBars(proc, 1, 0, viewport.worldWidth);
	if (viewport) {
		viewport.addChild(container);
		application?.stage.addChild(viewport);
		viewport.fitHeight(1000);
		viewport.scale.x = w / viewport.worldWidth;

		if (loadProc) {
			viewport.addEventListener("zoomed-end", updateLeafNodes);
			viewport.addEventListener("drag-end", updateLeafNodes);
			viewport.addEventListener("moved", fixText);
			updateLeafNodes();
			fixText();
		}
		viewport.plugins.get("timeline-ruler")?.resize();
	}
}

const barHeight = $derived(Math.max(h ?? 0, 1000) / 25);

const leafNodes: {
	proc: T;
	level: number;
	offset: number;
	width: number;
	bar: pixi.Graphics;
	color: string;
}[] = [];

const parentNodes: {
	proc: T;
	level: number;
	offset: number;
	width: number;
	bar: pixi.Graphics;
	color: string;
}[] = [];

//#region text
let fontFamily: string | undefined = variables.get("--vscode-font-family");
pixi.BitmapFontManager.install({
	chars: pixi.resolveCharacters(
		"!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[]^_`abcdefghijklmnopqrstuvwxyz{|}~ ",
	),
	name: "vscode-font",
	resolution: 3,
	style: {
		fontFamily: fontFamily,
		fontSize: 16,
	},
});
$effect(() => {
	if (variables.get("--vscode-font-family") !== fontFamily) {
		pixi.BitmapFontManager.uninstall("vscode-font");
		fontFamily = variables.get("--vscode-font-family");
		pixi.BitmapFontManager.install({
			chars: pixi.resolveCharacters(
				"!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[]^_`abcdefghijklmnopqrstuvwxyz{|}~ ",
			),
			name: "vscode-font",
			resolution: 2,
			style: {
				fontFamily: fontFamily,
				fontSize: 16,
			},
		});
	}
});

const textNodes = new Map<pixi.BitmapText, pixi.Graphics>();
function fixText() {
	if (!viewport || !container) {
		return;
	}
	const bounds = viewport.getVisibleBounds();
	for (const [textNode, mask] of textNodes.entries()) {
		if (textNode.destroyed || mask.destroyed) {
			textNodes.delete(textNode);
			continue;
		}
		textNode.scale.x = 1 / viewport.scale.x;
		textNode.scale.y = 1 / viewport.scale.y;
		textNode.x = Math.max(bounds.x, mask.bounds.x);
	}
}
//#endregion

//#region bar generation
async function addBars(
	proc: T,
	level = 1,
	offset = 0,
	width: number | undefined = undefined,
	color?: string,
) {
	width ??= viewport?.worldWidth ?? w ?? 1920;

	while (viewport && level * barHeight * 2 + 100 > viewport.worldHeight) {
		viewport.worldHeight += barHeight;
	}

	color ??= getProcessColor(
		getBaseColor(proc),
		1,
		Array.isArray(proc.children) ? proc.children.length : proc.children,
		level + 1,
		0,
	);
	const barContainer = new pixi.Container({ zIndex: -1 });
	container!.addChild(barContainer);
	const barContext = new pixi.GraphicsContext()
		.rect(offset, level * barHeight, width, barHeight)
		.fill({ color })
		.stroke({
			color: variables.get("--vscode-panel-background"),
			pixelLine: true,
		});

	const bar = new pixi.Graphics(barContext);

	const mask = new pixi.Graphics(barContext);
	barContainer.addChild(mask);
	const text = new pixi.BitmapText({
		text: " " + getName(proc),
		style: new pixi.TextStyle({
			fontFamily: "vscode-font",
			fontSize: 16,
			lineHeight: barHeight,
			stroke: { width: 0 },
			fill: { color: "#ffffff" },
			align: "left",
		}),
		x: offset,
		y: level * barHeight + 2,
		mask,
		scale: {
			x: 1 / (viewport?.scale.x ?? 1),
			y: 1 / (viewport?.scale.y ?? 1),
		},
		zIndex: 1,
	});

	barContainer.addChild(text);
	textNodes.set(text, mask);
	bar.eventMode = "static";
	bar.addEventListener("click", async () => {
		modalNode = proc;
		modalOpen = true;
		if (loadProc && typeof proc.children === "number") {
			proc = await withAbort(
				loadProc(proc),
				AbortSignal.any([AbortSignal.timeout(60000), setRootAbort.signal]),
			);
			await addBars(proc, level, offset, width, color);
			barContainer.destroy({ children: true });
		}
	});
	bar.addEventListener("rightclick", () => {
		setRootAbort.abort();
		setRoot?.(proc);
		setRootAbort = new AbortController();
	});
	bar.addEventListener("mouseenter", () => {
		tooltipNode = proc;
		bar.tint = 0x909090;
	});
	bar.addEventListener("mouseleave", () => {
		tooltipNode = undefined;
		bar.tint = 0xffffff;
	});
	barContainer.addChild(bar);

	if (Array.isArray(proc.children)) {
		parentNodes.push({ proc, level, offset, width, bar, color });
		const fullWidth = Math.max(
			proc.children.reduce((sum, child) => sum + child.etime, 0),
			proc.etime,
		);
		let childOffset = offset;
		let i = 0;
		const promises = [];
		for (const child of proc.children.toSorted(
			childrenComparator ?? ((a, b) => a.etime - b.etime),
		)) {
			const childWidth = width * (child.etime / fullWidth);
			promises.push(
				addBars(
					child,
					level + 1,
					childOffset,
					childWidth,
					getProcessColor(
						getBaseColor(child),
						child.etime / fullWidth,
						Array.isArray(child.children)
							? child.children.length
							: child.children,
						level + 1,
						i++,
					),
				),
			);
			childOffset += childWidth;
		}
		await Promise.allSettled(promises);
	} else {
		leafNodes.push({ proc, level, offset, width, bar, color });
	}
}
//#endregion

//#region modal
let modalNode = $state<T>();
let modalOpen = $state(false);
//#endregion

let tooltipNode = $state<T>();
</script>
<div class="w-full h-full" bind:clientWidth={w} bind:clientHeight={h}>
	<MousePopover visible={!!tooltipNode}>
		{#if tooltipNode}
			{@render tooltip(tooltipNode)}
		{/if}
	</MousePopover>
	{#if w && h}
		<Application
			bind:instance={application}
			backgroundAlpha={0}
			width={$state.snapshot(w)}
			height={$state.snapshot(h)}
			antialias={false}
			oninit={init}
		>
			<Container bind:instance={container} />
		</Application>
	{/if}
</div>
{#if modalNode}
	{@render modal(modalNode, modalOpen)}
{/if}