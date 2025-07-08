import { EventualResult } from "@opliko/eventual-result";
import {
	type CancellationToken,
	type Event,
	EventEmitter,
	FileDecoration,
	type FileDecorationProvider,
	FileType,
	ThemeColor,
	type Uri,
	workspace,
} from "vscode";
import type { Mapping } from "../mapping/mapping";

const fs = workspace.fs;
export class MappingDecorationProvider implements FileDecorationProvider {
	//#region properties
	#mapping?: Mapping;

	set mapping(mapping: Mapping) {
		this.#mapping = mapping;
		this._onDidChangeDecorations.fire(mapping.mapped);
	}

	private readonly _onDidChangeDecorations = new EventEmitter<Uri[]>();

	onDidChangeFileDecorations?: Event<Uri | Uri[] | undefined> =
		this._onDidChangeDecorations.event;
	//#endregion
	//#region initialization
	constructor(mapping?: Mapping) {
		this.#mapping = mapping;
		if (this.#mapping) {
			this._onDidChangeDecorations.fire(this.#mapping.mapped);
		}
	}
	//#endregion

	async provideFileDecoration(
		uri: Uri,
		_token: CancellationToken,
	): Promise<FileDecoration | undefined> {
		if (
			await new EventualResult(fs.stat(uri)).mapOr(
				false,
				(stat) => stat.type !== FileType.File,
			)
		) {
			return;
		}
		const mapped = this.#mapping?.isMapped(uri);
		return new FileDecoration(
			undefined,
			mapped ? "mapped" : "unmapped",
			!mapped
				? new ThemeColor("gitDecoration.ignoredResourceForeground")
				: new ThemeColor("gitDecoration.submoduleResourceForeground"),
		);
	}
}
