import { ExtensionContext, languages, window, workspace } from "vscode";
import { DBProvider } from "../db";
import { ActionProvider } from "./actions";
import { CasFileDecorationProvider } from "./decorations";
import { FileData } from "./file_data";

export function registerEnrichment(context: ExtensionContext, db: DBProvider) {
	FileData.init(context, db).then((fd) =>
		context.subscriptions.push(
			window.registerFileDecorationProvider(new CasFileDecorationProvider(fd)),
		),
	);
	context.subscriptions.push(
		languages.registerCodeLensProvider(
			{
				scheme: "file",
			},
			new ActionProvider(context, db),
		),
	);
}
