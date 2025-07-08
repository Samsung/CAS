import {
	CancellationToken,
	DocumentSymbol,
	DocumentSymbolProvider,
	Position,
	Range,
	SymbolInformation,
	SymbolKind,
	TextDocument,
} from "vscode";
import type { OpenGrokApi } from ".";

export class OGDocumentSymbolProvider implements DocumentSymbolProvider {
	#og: OpenGrokApi;
	constructor(og: OpenGrokApi) {
		this.#og = og;
	}
	async provideDocumentSymbols(
		document: TextDocument,
		token: CancellationToken,
	): Promise<SymbolInformation[] | DocumentSymbol[]> {
		const abort = new AbortController();
		token.onCancellationRequested(abort.abort);
		const symbols = await this.#og.getDefinitions(
			this.#og.toProjectUri(document.uri, "android"),
			abort.signal,
		);
		const documentSymbols = new Map(
			symbols.map((symbol) => [
				`${symbol.symbol}_${this.toSymbolKind(symbol.type)}`,
				new DocumentSymbol(
					symbol.symbol,
					symbol.signature ?? symbol.type,
					this.toSymbolKind(symbol.type),
					new Range(
						new Position(symbol.line - 1, symbol.lineStart),
						new Position(symbol.line - 1, symbol.lineEnd),
					),
					new Range(
						new Position(symbol.line - 1, symbol.lineStart),
						new Position(symbol.line - 1, symbol.lineEnd),
					),
				),
			]),
		);
		const children: string[] = [];
		for (const symbol of symbols) {
			if (symbol.signature) {
				const type = this.toSymbolKind(symbol.type);
				for (const parentType of this.getParentType(type)) {
					const parent = documentSymbols.get(
						`${symbol.signature}_${parentType}`,
					);
					if (parent && parent.name !== symbol.symbol) {
						parent.children.push(
							documentSymbols.get(`${symbol.symbol}_${type}`)!,
						);
						children.push(symbol.symbol);
					}
				}
			}
		}
		return [
			...documentSymbols
				.values()
				.filter((symbol) => !children.includes(symbol.name)),
		];
	}
	toSymbolKind(type: string): SymbolKind {
		switch (type) {
			case "member":
				return SymbolKind.Field;
			case "argument":
				return SymbolKind.TypeParameter;
			default:
				return (
					SymbolKind[
						Object.keys(SymbolKind).find(
							(kind) => kind.toLowerCase() === type.toLowerCase(),
						) as unknown as keyof typeof SymbolKind
					] ?? SymbolKind.Null
				);
		}
	}
	getParentType(type: SymbolKind): SymbolKind[] {
		switch (type) {
			case SymbolKind.EnumMember:
				return [SymbolKind.Enum];
			case SymbolKind.Method:
			case SymbolKind.Property:
			case SymbolKind.Field:
			case SymbolKind.Constructor:
				return [
					SymbolKind.Class,
					SymbolKind.Struct,
					SymbolKind.Object,
					SymbolKind.Enum,
				];
			default:
				return Object.values(SymbolKind).filter((v) => typeof v === "number");
		}
	}
}
