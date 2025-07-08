<script lang="ts" generics="T extends ProcessLike">
import chroma from "chroma-js";
import * as pixi from "pixi.js";
import { type IScale, Viewport } from "pixi-viewport";
import { type Snippet, tick } from "svelte";
import { Application, Container } from "svelte-pixi";
import { observeCssVar } from "./vscode/themeObserver.svelte";
import "pixi.js/unsafe-eval";
import { debounce } from "lodash-es";
import MousePopover from "./MousePopover.svelte";
import { ThreadIdRulerPlugin } from "./ThreadIDRuler.svelte";
import { TimelineRulerPlugin } from "./TimelineRuler.svelte";
import { trackEvent } from "./telemetry";

interface ProcessLike {
	children: this[] | number;
	stime?: number;
	etime: number;
}

interface Props<T extends ProcessLike> {
	proc: T;
	threads: number[];
	getName: (proc: T) => string;
	getBaseColor: (proc: T) => string;
	tooltip: Snippet<[T]>;
	modal: Snippet<[T, boolean]>;
	childrenComparator?: (a: T, b: T) => number;
}

const {
	proc,
	threads,
	getName,
	getBaseColor,
	tooltip,
	modal,
	childrenComparator,
}: Props<T> = $props();

const variables = observeCssVar(
	"--vscode-panel-background",
	"--vscode-font-family",
	"--vscode-charts-foreground",
	"--vscode-charts-blue",
	"--vscode-charts-yellow",
	"--vscode-charts-green",
	"--vscode-charts-red",
	"--vscode-editorRuler-foreground",
).variables;

// Derived sorted children using comparator or default stime/etime sort
const children = $derived(
	Array.isArray(proc.children)
		? splitIntoTracks(
				proc.children.toSorted(
					childrenComparator ??
						((a, b) => (a.stime ?? 0) - (b.stime ?? 0) || a.etime - b.etime),
				),
			)
		: [],
);

const start = $derived(
	typeof proc.children === "object"
		? proc.children
				.flatMap((t) => t.stime)
				.reduce((a, b) => Math.min(a ?? 0, b ?? 0), Infinity)
		: proc.stime,
);

function splitIntoTracks(processes: T[]) {
	if (processes.length === 0) {
		return [];
	}
	const tracks: { [key: number]: T[] } = [];
	let autosplit = false;
	if (!threads.length) {
		autosplit = true;
	}
	for (const process of processes) {
		let added = false;
		if (
			(process as unknown as { cpus: { cpu: number; timestamp: number }[] })
				.cpus
		) {
			let p = { stime: process.stime, etime: process.etime };
			for (const cputime of (
				process as unknown as { cpus: { c: number; t: number }[] }
			).cpus) {
				p.etime = cputime.t - (p.stime ?? 0);
				p = {
					...process,
					stime: cputime.t,
					etime: (process.stime ?? 0) + process.etime - (p.stime ?? 0),
				};
				if (!tracks[cputime.c]) {
					tracks[cputime.c] = [];
				}
				tracks[cputime.c].push(p as T);
			}
		} else {
			let foundTrack = false;
			for (const track of Object.values(tracks)) {
				if (
					(track.at(-1)?.stime ?? 0) + (track.at(-1)?.etime ?? 0) <=
					(process.stime ?? 0)
				) {
					track.push(process);
					foundTrack = true;
					break;
				}
			}
			if (!foundTrack) {
				tracks[Object.keys(tracks).length] = [process];
			}
		}
	}
	return tracks;
}

let w = $state<number>();
let h = $state<number>();
let application = $state<pixi.Application>();
let container = $state<pixi.Container>();
let viewport: Viewport | undefined = undefined;

const barHeight = $derived(Math.max(h ?? 0, 1000) / 20);

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
		textNode.scale.x = 1 / viewport!.scale.x;
		textNode.scale.y = 1 / viewport!.scale.y;
		textNode.x = Math.max(bounds.x, mask.bounds.x);
	}
}
//#endregion

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

const worldWidth = 2 ** 50;
const padding = 0;
async function init() {
	while (!application || !container || !w || !h) {
		await tick();
	}
	viewport = new Viewport({
		worldHeight: barHeight * threads.length,
		worldWidth: worldWidth,
		screenHeight: h,
		screenWidth: w,
		events: application.renderer.events,
	})
		.drag()
		.pinch()
		.wheel({
			axis: "x",
		})
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
	let i = 1;
	const widthScale = worldWidth / proc.etime;
	for (const [thread, track] of Object.entries(children)) {
		const trackContainer = new pixi.Container({
			x: barHeight / viewport.scale.x,
			y: (barHeight + padding) * i,
			width: worldWidth,
			zIndex: -1,
		});
		if (!track) {
			continue;
		}

		for (const child of track) {
			const barContainer = new pixi.Container({
				zIndex: -1,
			});
			const procBar = new pixi.Graphics({
				zIndex: 0,
			})
				.rect(
					((child.stime ?? 0) - (start ?? 0)) * widthScale,
					0,
					child.etime * widthScale,
					barHeight,
				)
				.fill({ color: chroma.random().hex() });
			const mask = new pixi.Graphics({
				zIndex: 0,
			})
				.rect(
					((child.stime ?? 0) - (start ?? 0)) * widthScale,
					0,
					child.etime * widthScale,
					barHeight,
				)
				.fill();
			const text = new pixi.BitmapText({
				text: " " + getName(child),
				// ... rest of text config
				style: new pixi.TextStyle({
					fontFamily: "vscode-font",
					fontSize: 18,
					lineHeight: barHeight,
					stroke: { width: 0 },
					fill: { color: "#ffffff" },
					align: "left",
				}),
				x: ((child.stime ?? 0) - (start ?? 0)) * widthScale,
				y: (barHeight - 20) / 2,
				mask,
				scale: {
					x: 1 / (viewport?.scale.x ?? 1),
					y: 1 / (viewport?.scale.y ?? 1),
				},
				zIndex: 1,
			});
			barContainer.addChild(mask);
			barContainer.addChild(text);
			textNodes.set(text, mask);
			procBar.eventMode = "static";
			procBar.addEventListener("mouseenter", () => {
				tooltipContent = child;
				procBar.tint = 0x909090;
			});
			procBar.addEventListener("mouseleave", () => {
				tooltipContent = undefined;
				procBar.tint = 0xffffff;
			});
			procBar.addEventListener("click", () => {
				modalNode = child;
				modalOpen = true;
			});
			barContainer.addChild(procBar);
			trackContainer.addChild(barContainer);
		}
		container.addChild(trackContainer);
		i++;
	}
	viewport.addChild(container);
	application.stage.addChild(viewport);
	viewport.fitHeight(
		Math.min(Math.max(h ?? 0, 1000), barHeight * threads.length),
	);
	viewport.scale.x = w / viewport.worldWidth;
	viewport.addEventListener("moved", fixText);
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
	viewport.plugins.add(
		"proc-ruler",
		new ThreadIdRulerPlugin(
			viewport,
			viewport,
			threads,
			barHeight / 2,
			timelineColors,
			barHeight,
		),
	);

	fixText();
}

let tooltipContent = $state<T>();

let modalNode = $state<T>();
let modalOpen = $state(false);
</script>
	
<div class="w-full h-full" bind:clientWidth={w} bind:clientHeight={h}>
	<MousePopover visible={!!tooltipContent}>
		{#if tooltipContent}
			{@render tooltip(tooltipContent)}
		{/if}
	</MousePopover>
	
	{#if w && h}
		<Application bind:instance={application} backgroundAlpha={0} width={$state.snapshot(w)} height={$state.snapshot(h)} antialias={false} oninit={init}>
			<Container bind:instance={container} />
		</Application>
	{/if}
</div>

{#if modalNode}
	{@render modal(modalNode, modalOpen)}
{/if}