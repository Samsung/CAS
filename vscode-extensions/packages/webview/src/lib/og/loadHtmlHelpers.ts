import {
	transformerRemoveLineBreak,
	transformerRenderWhitespace,
} from "@shikijs/transformers";
import flourite from "flourite";
import { extname } from "pathe";
import QuickLRU from "quick-lru";
import type { HighlighterCore } from "shiki";
import { type ShikiTransformer } from "shiki/core";
import languages from "$lib/og/languages.json";
import { highlighter } from "./shiki-bundle";

export function transformerHideFarLines(
	lines: number[],
	maxDistance = 2,
): ShikiTransformer {
	const includedLines = new Set(
		lines.flatMap((i) =>
			Array.from(
				{ length: maxDistance * 2 - 1 },
				(_, j) => i - maxDistance + j + 1,
			),
		),
	);
	let i = 1;
	let changed = false;
	return {
		line(node, line) {
			this.addClassToHast(node, "block");
			node.properties["data-line"] = line;
			this.addClassToHast(node, `code-group-${i}`);
			if (!includedLines.has(line)) {
				if (!changed) {
					i++;
				}
				changed = true;
				this.addClassToHast(node, "hiddenButAccessible");
				return {
					type: "element",
					children: [],
					tagName: "void",
					properties: {},
				};
			} else {
				changed = false;
			}
			return node;
		},
		postprocess(html) {
			// remove hidden lines
			return html.replaceAll("<void></void>", "");
		},
	};
}

const regexCache = new QuickLRU<string, RegExp>({
	maxSize: 100,
});

export function transformerHighlightWords(fullQuery: string): ShikiTransformer {
	let wordsRegex = regexCache.get(fullQuery);
	if (!wordsRegex) {
		wordsRegex = new RegExp(
			fullQuery
				.split(" ")
				.filter((s) => s.length)
				// escape regex symbols
				.map((s) => s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&"))
				.join("|"),
			"gmiu",
		);
		regexCache.set(fullQuery, wordsRegex);
	}
	return {
		postprocess(html) {
			html = html.replaceAll(
				wordsRegex,
				'<span class="highlighted-word">$&</span>',
			);
			return html;
		},
	};
}

export function detectLanguage(
	path: string,
	file: string,
	highlighter: HighlighterCore,
) {
	const ext = extname(path);
	if (ext.length > 1 && ext.slice(1) in languages) {
		const extLang =
			languages[ext.slice(1) as keyof typeof languages].toLowerCase();
		if (highlighter.getLoadedLanguages().includes(extLang)) {
			return extLang;
		}
	}
	const lang = flourite(file, {
		heuristic: true,
		shiki: true,
	}).language;
	return lang === "unknown" ? "text" : lang;
}

export async function loadHtml(
	path: string,
	file: string,
	lines: number[],
	theme: string,
	fullQuery: string,
) {
	return (await highlighter).codeToHtml(file, {
		lang: detectLanguage(path, file, await highlighter),
		theme,
		transformers: [
			transformerHideFarLines(lines, 3),
			transformerRemoveLineBreak(),
			transformerRenderWhitespace(),
			transformerHighlightWords(fullQuery),
		],
	});
}
