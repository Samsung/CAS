import { getLogger } from "@logtape/logtape";
import {
	CancellationToken,
	Location,
	Position,
	Range,
	ReferenceContext,
	ReferenceProvider,
	TextDocument,
} from "vscode";
import type { OpenGrokApi } from ".";
export class OGReferenceProvider implements ReferenceProvider {
	#og: OpenGrokApi;
	#logger = getLogger(["CAS", "OG", "ReferenceProvider"]);
	constructor(og: OpenGrokApi) {
		this.#og = og;
	}
	async provideReferences(
		document: TextDocument,
		position: Position,
		_context: ReferenceContext,
		token: CancellationToken,
	): Promise<Location[]> {
		const abort = new AbortController();
		token.onCancellationRequested(abort.abort);
		abort.signal.addEventListener("abort", () => {
			this.#logger.debug("aborted");
		});
		const projects = (document.uri.fragment || "android").split(",");
		const results = await this.#og.search(
			{
				symbol: document.getText(document.getWordRangeAtPosition(position)),
				projects,
				maxresults: 10,
			},
			abort.signal,
		);
		return (
			await Promise.all(
				Object.entries(results.results).map(([path, value]) =>
					Promise.all(
						value.map(async (result) => ({
							uri: await this.#og.toDocumentUri(path, projects, token),
							range: new Range(
								new Position(result.lineNumber - 1, result.line.indexOf("<b>")),
								new Position(
									result.lineNumber - 1,
									result.line.indexOf("</b>") - 3,
								),
							),
						})),
					),
				),
			)
		).flat();
	}
}
