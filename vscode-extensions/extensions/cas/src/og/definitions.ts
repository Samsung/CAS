import {
	CancellationToken,
	Definition,
	DefinitionLink,
	DefinitionProvider,
	Position,
	Range,
	TextDocument,
} from "vscode";
import type { OpenGrokApi } from ".";

export class OGDefinitionProvider implements DefinitionProvider {
	#og: OpenGrokApi;
	constructor(og: OpenGrokApi) {
		this.#og = og;
	}
	async provideDefinition(
		document: TextDocument,
		position: Position,
		token: CancellationToken,
	): Promise<Definition | DefinitionLink[]> {
		const abort = new AbortController();
		token.onCancellationRequested(abort.abort);
		const projects = (document.uri.fragment.slice(1) || "android").split(",");
		const results = await this.#og.search(
			{
				def: document.getText(document.getWordRangeAtPosition(position)),
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
							targetUri: await this.#og.toDocumentUri(path, projects, token),
							targetRange: new Range(
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
