import { FileType } from "@cas/types/bas.js";
import {
	Event,
	EventEmitter,
	FileDecoration,
	FileDecorationProvider,
	ThemeColor,
	Uri,
} from "vscode";
import { FileData } from "./file_data";

const fileColors: Record<FileType, ThemeColor | undefined> = {
	compiled: new ThemeColor("symbolIcon.classForeground"),
	linked: new ThemeColor("textLink.foreground"),
	plain: undefined,
};

export class CasFileDecorationProvider implements FileDecorationProvider {
	changedFileDecorations = new EventEmitter<Uri | Uri[] | undefined>();
	onDidChangeFileDecorations: Event<Uri | Uri[] | undefined> =
		this.changedFileDecorations.event;
	fileData: FileData;
	constructor(fd: FileData) {
		this.fileData = fd;
	}
	provideFileDecoration(uri: Uri): FileDecoration {
		const data = this.fileData.get(uri);
		return {
			badge: data?.access,
			tooltip: `${data?.type ? `${data.type} file ${data?.link ? "(linked) •" : "•"} ` : ""}${data?.access ? `open mode: ${data.access} ` : ""}`,
			color: fileColors[data?.type ?? "plain"],
		};
	}
}
