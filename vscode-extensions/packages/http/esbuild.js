/*
Modified from code from https://code.visualstudio.com/api/working-with-extensions/bundling-extension
Original licensed under the MIT License Copyright © 2025 Microsoft
*/
import esbuild  from "esbuild";

const production = process.argv.includes("--production");
const watch = process.argv.includes("--watch");

/**
 * @type {import('esbuild').Plugin}
 */
const esbuildProblemMatcherPlugin = {
	name: "esbuild-problem-matcher",

	setup(build) {
		build.onStart(() => {
			console.log("[watch] build started");
		});
		build.onEnd((result) => {
			for (const { text, location } of result.errors) {
				console.error(`✘ [ERROR] ${text}`);
				console.error(
					`    ${location.file}:${location.line}:${location.column}:`,
				);
			}
			console.log("[watch] build finished");
		});
	},
};

async function main() {
	const ctx = await esbuild.context({
		entryPoints: ["src/index.ts"],
		bundle: true,
		format: "esm",
		minify: production,
		sourcemap: !production,
		sourcesContent: false,
		platform: "node",
		outfile: "dist/http.js",
		external: ["vscode"],
		logLevel: "silent",
		plugins: [
			/* add to the end of plugins array */
			esbuildProblemMatcherPlugin,
		],
		target: ["node20"],
	});
	if (watch) {
		await ctx.watch();
	} else {
		await ctx.rebuild();
		await ctx.dispose();
	}
}

main().catch((e) => {
	console.error(e);
	process.exit(1);
});
