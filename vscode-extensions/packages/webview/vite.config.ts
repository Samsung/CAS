import { paraglideVitePlugin } from '@inlang/paraglide-js';
import { sveltekit } from "@sveltejs/kit/vite";
import { defineConfig, type PluginOption } from "vite";
import copy from 'rollup-plugin-copy'
import tailwindcss from '@tailwindcss/vite'

export default defineConfig(({mode}) => {
	process.env.VITE_MODE = mode;
	return {
	plugins: [
		paraglideVitePlugin({ project: "./project.inlang", outdir: "./src/lib/paraglide", strategy: ['url', 'preferredLanguage', 'baseLocale'], }),
		sveltekit(),
		copy({
			hook: "buildStart",
			targets: [
				{ src: ["node_modules/@vscode/codicons/dist/codicon.ttf", "node_modules/@vscode/codicons/dist/codicon.css"], dest: "static"}
			]
		}),
		tailwindcss(),
	],
	server: {
		
		// warmup: {
		// 	clientFiles: ["src/**/*.svelte", "src/lib/**/*.ts", "src/lib/**/*.js", "src/routes/**/*.ts"],
		// }
		cors: {
			origin: /vscode-webview:\/\/.+/
		}
	},
	build: {
		target: "chrome128",
	},
	worker: {
		format: "es",
		plugins: () => [useNameAsBase()],
	},
	ssr: {
		noExternal: ["pixi-viewport"],
		target: "node"
	},
	optimizeDeps: {
		esbuildOptions: {
			loader: {
				".vert": "text",
				".frag": "text",
			},
		},	
	},
	
}});

function useNameAsBase(): PluginOption {
	return {
		renderDynamicImport({ moduleId }: { moduleId: string }) {
			if (moduleId.includes("shiki")) {
				return {
					left: "import(`${globalThis.name}${",
					right: "}`)",
				};
			}
			return null;
		},
	} as PluginOption;
}
