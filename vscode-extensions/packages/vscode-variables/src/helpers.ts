import { dirname } from "path";
import { WorkspaceFolder, window } from "vscode";

export function currentFilePath() {
	return window.activeTextEditor?.document.uri.fsPath;
}

export function relativePath(path: string, folder: string): string {
	return path.startsWith(path) ? path.slice(folder.length) : path;
}
