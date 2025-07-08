import type {
	Command,
	SourceControlResourceDecorations,
	SourceControlResourceState,
	Uri,
} from "vscode";

export class P4ResourceState implements SourceControlResourceState {
	resourceUri: Uri;
	command?: Command;
	decorations?: SourceControlResourceDecorations;
	contextValue?: string;

	constructor(resourceUri: Uri, exists?: boolean, contextValue?: string) {
		this.resourceUri = resourceUri;
		this.command = {
			command: "cas-p4.openFile",
			arguments: [this.resourceUri],
			title: "Open File",
		};
		if (exists !== undefined) {
			this.setDecoration(exists);
		}
		this.contextValue = contextValue;
	}

	setDecoration(exists: boolean): SourceControlResourceDecorations {
		this.decorations = {
			faded: !exists,
			strikeThrough: !exists,
		};
		return this.decorations;
	}
}
