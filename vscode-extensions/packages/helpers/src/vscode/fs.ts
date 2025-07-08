import { Uri, workspace } from "vscode";
import { decodeText } from "../text";

const { readFile } = workspace.fs;

export async function readJSON<T>(path: Uri | string): Promise<T> {
	if (typeof path === "string") {
		path = Uri.file(path);
	}
	return JSON.parse(decodeText(await readFile(path))) as T;
}
