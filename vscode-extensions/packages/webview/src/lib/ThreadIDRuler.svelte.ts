import * as PIXI from "pixi.js";
import { Plugin, Viewport } from "pixi-viewport";
import type { TimelineColors } from "$lib/TimelineRuler.svelte";

export class ThreadIdRulerPlugin extends Plugin {
	private graphics: PIXI.Graphics;
	private threadIds: number[];
	private colors: TimelineColors = $state({
		textColor: 0xffffff,
		lineColor: 0x666666,
		backgroundColor: 0x000000,
	});
	private height: number;
	private rulerWidth: number;
	private labelContainer: PIXI.Container;

	constructor(
		viewport: Viewport,
		container: PIXI.Container,
		threadIds: number[],
		rulerWidth: number,
		colors: TimelineColors,
		height: number,
	) {
		super(viewport);
		this.threadIds = threadIds;
		this.colors = colors;
		this.rulerWidth = rulerWidth;

		this.graphics = new PIXI.Graphics({
			zIndex: 10,
		});

		this.labelContainer = new PIXI.Container({
			zIndex: 11,
		});
		this.height = height;

		container.addChild(this.graphics);
		container.addChild(this.labelContainer);

		this.parent.addEventListener("zoomed", () => this.draw());
		this.parent.addEventListener("moved", () => this.draw());
		this.draw();
	}

	public resize(): void {
		this.draw();
	}

	private draw() {
		this.graphics.clear();
		this.labelContainer.removeChildren();

		// Draw background
		this.graphics
			.rect(
				0,
				this.height,
				this.rulerWidth / this.parent.scale.x,
				this.parent.worldHeight,
			)
			.fill({ color: this.colors.backgroundColor, alpha: 0.85 });

		const visibleTop = this.parent.top;
		const visibleBottom = this.parent.bottom;
		const xPos = this.parent.left;
		let i = 1;
		for (const thread of this.threadIds) {
			const yPos = i * this.height; // Adjust this based on your actual thread spacing

			// Only draw if within visible area
			if (yPos >= visibleTop && yPos <= visibleBottom) {
				// Draw horizontal tick line
				this.graphics
					.moveTo(xPos, yPos)
					.lineTo(xPos + this.rulerWidth / this.parent.scale.x, yPos)
					.stroke({
						color: this.colors.lineColor,
						pixelLine: true,
					});

				// Create thread ID label
				const text =
					this.labelContainer.children.length > i &&
					this.labelContainer.getChildAt(i) instanceof PIXI.Text
						? this.labelContainer.getChildAt<PIXI.Text>(i)
						: this.labelContainer.addChild(
								new PIXI.Text({
									text: thread.toString(),
									style: new PIXI.TextStyle({
										fontFamily: "vscode-font",
										fontSize: 16,
										fill: { color: this.colors.textColor },
									}),
									x: xPos,
									y: yPos,
									height: this.height,
									width: this.rulerWidth / (this.parent.scale.x ?? 1),
									scale: {
										x: 1 / (this.parent.scale.x ?? 1),
										y: 1,
									},
									zIndex: 20,
								}),
							);
				text.x = xPos;
				text.width = this.rulerWidth / (this.parent.scale.x ?? 1);
				text.scale.x = 1 / (this.parent.scale.x ?? 1);
			}
			i++;
		}
	}
}
