import { preprocessMeltUI, sequence } from "@melt-ui/pp";
import adapter from "@sveltejs/adapter-static";
import { vitePreprocess } from "@sveltejs/vite-plugin-svelte";
/** @type {import("@sveltejs/kit").Config}*/
const config = {
  // Consult https://kit.svelte.dev/docs/integrations#preprocessors
  // for more information about preprocessors
  preprocess: sequence([vitePreprocess(), preprocessMeltUI()]),
  kit: {
    adapter: adapter({
      pages: "dist",
      assets: "dist",
    }),
    paths: {
      relative: true,
    },
    csp: {
      mode: "hash",
      directives: {
        "script-src": [
          "self",
          "{{webview.cspSource}}",
          "nonce-{{webview.nonce}}",
          "sha256-FOpXssiAuZhMmxcanruy6rzBHBRG/+9kVQeH8A63LlQ=",
          "wasm-unsafe-eval",
          "https://*.vscode-resource.vscode-cdn.net",
        ],
        "worker-src": [
          "self",
          "{{webview.cspSource}}",
          "unsafe-inline",
          "data:",
          "blob:",
          "http://localhost:5173",
        ],
        "style-src": ["self", "{{webview.cspSource}}", "unsafe-inline"],
        "connect-src": [
          "self",
          "{{webview.cspSource}}",
          "{{webview.telemetryHost}}",
          "https://*.vscode-resource.vscode-cdn.net",
        ],
        "font-src": ["self", "{{webview.cspSource}}"],
      },
    },
    serviceWorker: {
      register: false,
    },
  },
  vitePlugin: {
    inspector: process.env.VITE_MODE === "development",
    prebundleSvelteLibraries: true,
  },
};
export default config;
