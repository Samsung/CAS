import { SvelteMap } from "svelte/reactivity";
import { browser } from "$app/environment";
import { mapToThemeId } from "./getThemeId";
export function observeThemeId() {
	let theme = $state("dark-plus");
	if (browser) {
		const body = document.getElementsByTagName("body")[0];
		const bodyChangeObserver = new MutationObserver(() => {
			theme = mapToThemeId(
				body.getAttribute("data-vscode-theme-id"),
				body.getAttribute("data-vscode-theme-kind"),
			);
		});
		bodyChangeObserver.observe(body, {
			attributeFilter: ["data-vscode-theme-id", "data-vscode-theme-kind"],
			attributeOldValue: false,
			attributes: true,
			characterData: false,
			childList: false,
			subtree: false,
		});
		theme = mapToThemeId(
			body.getAttribute("data-vscode-theme-id"),
			body.getAttribute("data-vscode-theme-kind"),
		);
	}
	// getter required to preserve reactivity across function boundry
	return {
		get theme() {
			return theme;
		},
	};
}

export function observeCssVar(...v: string[]) {
	let variables = $state(new SvelteMap<string, string>());
	if (browser) {
		const bodyChangeObserver = new MutationObserver(() => {
			for (const key of v) {
				variables.set(
					key,
					getComputedStyle(document.documentElement).getPropertyValue(key),
				);
			}
		});
		bodyChangeObserver.observe(document.documentElement, {
			attributeFilter: ["style"],
			attributeOldValue: false,
			attributes: true,
			characterData: false,
			childList: false,
			subtree: false,
		});
		for (const key of v) {
			variables.set(
				key,
				getComputedStyle(document.documentElement).getPropertyValue(key),
			);
		}
	}
	// getter required to preserve reactivity across function boundry
	return {
		get variables() {
			return variables;
		},
	};
}
