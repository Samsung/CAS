import {
	CancellationToken,
	EventEmitter,
	TextDocumentContentProvider,
	Uri,
} from "vscode";
import type { OpenGrokApi } from ".";

export class OGTextDocumentContentProvider
	implements TextDocumentContentProvider
{
	#onDidChange = new EventEmitter<Uri>();
	onDidChange = this.#onDidChange.event;

	#og: OpenGrokApi;
	constructor(og: OpenGrokApi) {
		this.#og = og;
	}
	async provideTextDocumentContent(
		uri: Uri,
		token: CancellationToken,
	): Promise<string> {
		const abort = new AbortController();
		token.onCancellationRequested(abort.abort);

		return this.#og.getFile(uri.path, abort.signal);
	}
}
