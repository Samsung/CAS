import * as PIXI from "pixi.js";
import { Plugin, Viewport } from "pixi-viewport";
import { nsFormat } from "$lib/helpers";

export interface TimelineColors {
	lineColor: number;
	textColor: number;
	backgroundColor: number;
}

export class TimelineRulerPlugin extends Plugin {
	private graphics: PIXI.Graphics;
	private startTime: number;
	private endTime: number;
	private fontSize: number = $state(16);
	private colors: TimelineColors = $state({
		textColor: 0xffffff,
		lineColor: 0x666666,
		backgroundColor: 0x000000,
	});
	private tickHeight: number;
	private tickContainer: PIXI.Container;

	constructor(
		viewport: Viewport,
		container: PIXI.Container,
		startTime: number,
		endTime: number,
		tickHeight: number,
		colors: TimelineColors,
	) {
		super(viewport);
		this.startTime = startTime;
		this.endTime = endTime;
		this.graphics = new PIXI.Graphics({
			height: 1000,
			zIndex: 10,
		});
		this.fontSize = 12;
		this.colors = colors;
		this.tickHeight = tickHeight;
		this.tickContainer = new PIXI.Container({
			width: this.parent.worldWidth,
			zIndex: 10,
		});
		container.addChild(this.tickContainer);
		this.tickContainer.addChild(this.graphics);
		this.parent.addEventListener("zoomed", () => this.draw());
		this.parent.addEventListener("moved", () => this.draw());
		this.draw();
	}
	public resize(): void {
		this.draw();
	}

	private draw() {
		this.graphics.clear();
		// prevent streaks
		this.tickContainer.children.forEach(
			(child) =>
				child instanceof PIXI.Text && child.destroy({ children: true }),
		);

		// Get visible area coordinates
		const worldWidth = this.parent.worldWidth;
		const viewWidth = Math.max(
			this.parent.width,
			this.parent.screenWidthInWorldPixels,
		);
		const totalDuration = this.endTime - this.startTime;
		this.graphics.rect(0, this.parent.top, worldWidth, this.tickHeight).fill({
			color: this.colors.backgroundColor,
			alpha: 0.85,
		});
		// Draw horizontal line
		this.graphics.moveTo(0, this.parent.top);
		this.graphics.lineTo(worldWidth, this.parent.top).stroke({
			pixelLine: true,
			color: this.colors.lineColor,
		});

		// Calculate tick intervals
		const visibleDuration = Math.min(
			(this.parent.screenWidthInWorldPixels / worldWidth) * totalDuration,
			totalDuration,
		);
		const interval = visibleDuration / 10; // this.calculateTickInterval(visibleDuration);

		const startX = this.parent.left;
		const startTime =
			(this.parent.left / worldWidth) * totalDuration + this.startTime;
		for (let i = 0; i < 11; i++) {
			const x = startX + (viewWidth / 10) * i;
			// Draw major tick

			this.graphics.moveTo(x, this.parent.top);
			this.graphics.lineTo(x, this.parent.top + this.tickHeight).stroke({
				pixelLine: true,
				color: this.colors.lineColor,
			});

			// Add time label
			const labelTime = Math.ceil(startTime + interval * i);
			const formattedTime =
				nsFormat(labelTime, this.getSmallestUnit(labelTime)) ?? "0ns";
			if (formattedTime) {
				const textWidth = viewWidth / 10;
				const text =
					this.tickContainer.children.length > i &&
					this.tickContainer.getChildAt(i) instanceof PIXI.Text
						? this.tickContainer.getChildAt<PIXI.Text>(i)
						: this.tickContainer.addChild(
								new PIXI.Text({
									text: formattedTime,
									style: new PIXI.TextStyle({
										fontFamily: "vscode-font",
										fontSize: 16,
										fill: { color: this.colors.textColor },
										align: "right",
										whiteSpace: "pre",
									}),
									x: x,
									y: this.parent.top,
									height: (this.tickHeight * 2) / 3,
									width: textWidth,
									scale: {
										x: 1 / (this.parent.scale.x ?? 1),
										y: 1,
									},
									anchor: {
										x: 1.05,
										y: -0.25,
									},
									zIndex: 11,
								}),
							);
				text.text = formattedTime;
				text.x = x;
				text.y = this.parent.top;
				text.scale.x = 1 / (this.parent.scale.x ?? 1);
			}
		}
	}
	private getSmallestUnit(interval: number, max: string = "second") {
		if (interval < 1_000 || max === "nanosecond") {
			return "nanosecond";
		}
		if (interval < 1_000_000 || max === "nanosecond") {
			return "microsecond";
		}
		if (interval < 1_000_000_000 || max === "nanosecond") {
			return "millisecond";
		}
		if (interval < 60_000_000_000 || max === "second") {
			return "second";
		}
		if (interval < 3600_000_000_000 || max === "minute") {
			return "minute";
		}
		return "hour";
	}
}
