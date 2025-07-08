import { setupApi } from "@cas/http";
import type * as vscode from "vscode";
import { getLogger, initLogger } from "./logger";
import { Main } from "./main";

const disposables: Disposable[] = [];

// This method is called when your extension is activated
export async function activate(context: vscode.ExtensionContext) {
	await initLogger(context);
	await setupApi(context);

	const logger = getLogger("init");
	logger.info("activating extension");
	const main = new Main(context);
	disposables.push(main);
	await main.initialChecks();
}

// This method is called when your extension is deactivated
export function deactivate() {
	for (const disposable of disposables) {
		disposable[Symbol.dispose]();
	}
}
