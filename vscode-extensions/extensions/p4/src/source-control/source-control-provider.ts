import { EventualResult, run } from "@opliko/eventual-result";
import {
	Command,
	type FileStat,
	QuickDiffProvider,
	type SourceControl,
	SourceControlInputBox,
	type SourceControlResourceGroup,
	scm,
	type Uri,
	workspace,
} from "vscode";
import { P4ResourceState } from "./resource-state";

const fs = workspace.fs;

export class P4SourceControlProvider {
	#scm: SourceControl;
	#mappedGroup: SourceControlResourceGroup;
	constructor(_root: Uri) {
		this.#scm = scm.createSourceControl("cas-p4", "CAS P4");
		this.#mappedGroup = this.#scm.createResourceGroup(
			"mapped",
			"P4 mapped paths",
		);
		this.#mappedGroup.hideWhenEmpty = true;
	}

	async addToGroup(files: Uri | Uri[]) {
		const fileArr = Array.isArray(files) ? files : [files];
		this.#mappedGroup.resourceStates = [
			...this.#mappedGroup.resourceStates,
			...fileArr.map((file) => new P4ResourceState(file)),
		];

		return this.redecorate();
	}

	async redecorate() {
		return Promise.all(
			this.#mappedGroup.resourceStates.map(async (resource) => {
				(resource as P4ResourceState).setDecoration(
					await EventualResult.fromPromise(
						fs.stat(resource.resourceUri) as Promise<FileStat>,
					).mapOr(false, () => true),
				);
			}),
		);
	}
}
