import { homedir } from "os";
import { parse, sep } from "path";
import QuickLRU from "quick-lru";
import { Range, Uri, WorkspaceFolder, window, workspace } from "vscode";
import { relativePath } from "./helpers";

export type Substitution =
	| string
	| (() => string)
	| ((
			activeWorkspaceFolder?: WorkspaceFolder,
			file?: Uri,
	  ) => string | undefined)
	| ((
			activeWorkspaceFolder?: WorkspaceFolder,
			file?: Uri,
			arg?: string,
	  ) => string | undefined);

export type AsyncSubstitution =
	| Substitution
	| (() => Promise<string>)
	| ((
			activeWorkspaceFolder?: WorkspaceFolder,
			file?: Uri,
	  ) => Promise<string | undefined>)
	| ((
			activeWorkspaceFolder?: WorkspaceFolder,
			file?: Uri,
			arg?: string,
	  ) => Promise<string | undefined>);

interface Variables<Async extends boolean> {
	[name: string]: Async extends true ? AsyncSubstitution : Substitution;
}
export type SyncVariables = Variables<false>;
export type AsyncVariables = Variables<true>;

interface GenericSubstitutionOptions<Async extends boolean> {
	customVars?: Variables<Async>;
	/**
	 * @default false
	 */
	recursive?: boolean;
	/**
	 * @default true
	 */
	defaults?: boolean;
	/**
	 * @default vscode.window.activeTextEditor?.document.uri
	 */
	file?: Uri;
	/**
	 * @default true
	 */
	memoize?: boolean;
}
export type SubstitutionOptions = GenericSubstitutionOptions<false>;
export type AsyncSubstitutionOptions = GenericSubstitutionOptions<true>;

const defaultVariables: SyncVariables = {
	userHome: homedir,
	workspaceFolder: (activeWorkspace?: WorkspaceFolder) =>
		activeWorkspace ? activeWorkspace.uri.fsPath : undefined,
	relativeFile: (activeWorkspace?: WorkspaceFolder, path?: Uri) =>
		activeWorkspace && path
			? relativePath(path.fsPath, activeWorkspace.uri.fsPath)
			: undefined,
	relativeFileDirname: (activeWorkspace?: WorkspaceFolder, path?: Uri) =>
		activeWorkspace && path
			? parse(relativePath(path.fsPath, activeWorkspace.uri.fsPath)).dir
			: undefined,
	fileBasename: (_?: WorkspaceFolder, path?: Uri) =>
		path ? parse(path.fsPath).base : undefined,
	fileBasenameNoExtension: (_?: WorkspaceFolder, path?: Uri) =>
		path ? parse(path.fsPath).name : undefined,
	fileExtname: (_?: WorkspaceFolder, path?: Uri) =>
		path ? parse(path.fsPath).ext : undefined,
	fileDirname: (_?: WorkspaceFolder, path?: Uri) =>
		path ? parse(path.fsPath).dir : undefined,
	fileDirnameBasename: (_?: WorkspaceFolder, path?: Uri) =>
		path ? parse(parse(path.fsPath).dir).base : undefined,
	cwd: (_?: WorkspaceFolder, path?: Uri) =>
		path ? parse(path.fsPath).dir : undefined,
	lineNumber: () =>
		window.activeTextEditor
			? (window.activeTextEditor.selection.start.line + 1).toString()
			: undefined,
	selectedText: () =>
		window.activeTextEditor
			? window.activeTextEditor.document.getText(
					new Range(
						window.activeTextEditor.selection.start,
						window.activeTextEditor.selection.end,
					),
				)
			: undefined,
	env: (_?: WorkspaceFolder, __?: Uri, arg?: string) =>
		arg ? process.env[arg] : undefined,
	config: (_?: WorkspaceFolder, __?: Uri, arg?: string) =>
		arg ? workspace.getConfiguration().get(arg)?.toString() : undefined,
	execPath: process.execPath,
	pathSeparator: sep,
	"/": sep,
};

const variableRegexp = /\$\{(?<variable>[^:\}]+)(:(?<argument>.+))?\}/giu;

const cache = new QuickLRU<string, string>({
	maxSize: 1000, // 1k entries should be enough for most use cases
	maxAge: 10000, // 10 seconds max age for each entry
});

export function resolve(input: string, options?: SubstitutionOptions): string {
	if (!input.replace) {
		return input;
	}
	let output: string | undefined;
	if ((options?.memoize ?? true) && (output = cache.get(input)) !== undefined) {
		return output;
	}
	const file = options?.file ?? window.activeTextEditor?.document.uri;

	const activeWorkspaceFolder =
		(file ? workspace.getWorkspaceFolder(file) : undefined) ??
		workspace.workspaceFolders?.[0];
	output = input.replace(variableRegexp, (...args) => {
		const groups = args.at(-1);
		if (typeof groups !== "object" || !groups.variable) {
			return "";
		}
		const value =
			options?.customVars?.[groups.variable] ??
			((options?.defaults ?? true) ? defaultVariables[groups.variable] : "") ??
			"";
		if (!value || typeof value === "string") {
			return value ?? "";
		}
		return (
			value(
				activeWorkspaceFolder,
				file ?? activeWorkspaceFolder?.uri,
				groups.argument,
			) ?? ""
		);
	});
	if (options?.recursive && variableRegexp.test(output)) {
		output = resolve(output, { ...options, memoize: false });
	}
	if (options?.memoize ?? true) {
		cache.set(input, output);
	}
	return output;
}

// TODO: resolve async
