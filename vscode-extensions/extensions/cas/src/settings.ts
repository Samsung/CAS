import { resolve } from "@cas/vscode-variables";
import { execSync } from "child_process";
import { existsSync, realpathSync } from "fs";
import { cpus } from "os";
import { dirname, normalize } from "path";
import { keyof } from "valibot";
import * as vscode from "vscode";
import { DBInfo } from "./db/index";
import { Snippets } from "./db/snippets";
import { debug } from "./logger";

export class Settings {
	readonly context: vscode.ExtensionContext;
	readonly id = "cas";

	//#region initialization
	constructor(context: vscode.ExtensionContext) {
		this.context = context;
		context.subscriptions.push(
			vscode.commands.registerCommand("cas.server.pick", () =>
				this.pickCasServer(),
			),
		);

		return new Proxy(this, {
			get(target, p: keyof Settings | (string & {}), receiver) {
				if (p in target || typeof p === "symbol") {
					return Reflect.get(target, p, receiver);
				}
				return target.get(p);
			},
			set(target, p: keyof Settings | (string & {}), newValue, receiver) {
				if (p in target && Object.getOwnPropertyDescriptor(target, p)?.set) {
					return Reflect.set(target, p, newValue, receiver);
				}
				target.set(p, newValue);
				return true;
			},
		});
	}
	get isGenerated() {
		return this.genCmd?.length ? true : false;
	}
	async initSettings() {
		vscode.commands.executeCommand(
			"setContext",
			"cas.isGenerated",
			this.isGenerated,
		);
		const cs: string | undefined = vscode.workspace
			.getConfiguration("cas")
			.get("server");
		if (!cs || !existsSync(cs)) {
			const serverCmd = this.guessCASServerBin();
			if (serverCmd) {
				await this.set(
					"server",
					serverCmd,
					vscode.ConfigurationTarget.Global,
				).then(() => {
					debug(`[cas.Settings] guessed cas.server = ${serverCmd}`);
				});
			}
		}

		for (const { name, command } of this.snippets) {
			Snippets.init(this.context).set(name, command);
		}

		debug(`[cas.Settings] isGenerated       = ${this.isGenerated}`);
		debug(`[cas.Settings] cas.basDatabase   = ${JSON.stringify(this.basDB)}`);
		debug(`[cas.Settings] cas.basDatabases  = ${JSON.stringify(this.basDBs)}`);
		debug(`[cas.Settings] cas.ftdbDatabase  = ${JSON.stringify(this.FTDB)}`);
		debug(`[cas.Settings] cas.ftdbDatabases = ${JSON.stringify(this.FTDBs)}`);
		debug(`[cas.Settings] cas.server        = ${this.casServer}`);
	}

	//#region generic get/set
	get<T>(name: string): T | undefined;
	get<T>(name: string, defaultValue: T): T;
	public get<T>(name: string, defaultValue?: T): T | undefined {
		const result = vscode.workspace
			.getConfiguration()
			.get(
				name.startsWith(`${this.id}.`) ? name : `${this.id}.${name}`,
				defaultValue,
			);
		if (typeof result === "string") {
			return this.resolve(result) as T;
		}
		if (Array.isArray(result)) {
			return result.map((r) => this.resolve(r)) as T;
		}
		if (result && typeof result === "object") {
			return Object.fromEntries(
				Object.entries(result).map(([key, value]) => [
					key,
					typeof value === "string" ? this.resolve(value) : value,
				]),
			) as T;
		}
		return result;
	}

	public resolve(input: string) {
		try {
			return resolve(input);
		} catch {
			return input;
		}
	}

	public set<T>(
		name: string,
		value?: T,
		target?: vscode.ConfigurationTarget | boolean,
		overrideInLanguage?: boolean,
	) {
		return vscode.workspace
			.getConfiguration()
			.update(
				name.startsWith(`${this.id}.`) ? name : `${this.id}.${name}`,
				value,
				target,
				overrideInLanguage,
			);
	}
	public update<T>(
		name: string,
		value?: T,
		target?: vscode.ConfigurationTarget | boolean,
		overrideInLanguage?: boolean,
	) {
		return this.set(name, value, target, overrideInLanguage);
	}

	public inspect(name: string) {
		return vscode.workspace
			.getConfiguration()
			.inspect(name.startsWith(`${this.id}.`) ? name : `${this.id}.${name}`);
	}
	public listen<T>(
		name: string,
		callback: (setting: T | undefined) => unknown,
	) {
		this.context.subscriptions.push(
			vscode.workspace.onDidChangeConfiguration((e) => {
				if (e.affectsConfiguration(`cas.${name}`)) {
					callback(this.get<T>(name));
				}
			}),
		);
	}
	//#endregion
	//#region setting getters
	public get casServer(): string | undefined {
		return this.get("server");
	}

	public get casServerPortRange(): string | undefined {
		return this.get("serverPortRange");
	}

	public get genCmd(): string | undefined {
		return this.get("genCmd");
	}

	public get projName(): string | undefined {
		return this.get("projectName");
	}

	public get baseDir(): string {
		return this.get("baseDir", Settings.getWorkspaceDir());
	}

	public get concurency(): number {
		return this.get("concurency", cpus().length);
	}

	public get basDB(): DBInfo | { path: undefined; scheme: undefined } {
		return (
			this.get("basDatabase", undefined) ?? {
				path: undefined,
				scheme: undefined,
			}
		);
	}

	public get basDBs(): DBInfo[] {
		return this.get("basDatabases", []);
	}

	public get FTDB(): DBInfo | { path: undefined; scheme: undefined } {
		return (
			this.get("ftdbDatabase", undefined) ?? {
				path: undefined,
				scheme: undefined,
			}
		);
	}

	public get FTDBs(): DBInfo[] {
		return this.get("ftdbDatabases", []);
	}

	public get telemetryHost(): string {
		// TODO: add a proper default once we have a hosted instance (also change in package.json)
		return this.get("telemetryHost", "http://localhost:8000");
	}

	public get telemetryEnabled(): boolean {
		return this.get("telemetryEnabled", true);
	}

	public get workspaceType(): string {
		return this.get("workspaceType", "type:local");
	}

	public get OGUrl(): string | undefined {
		return this.get("openGrokUrl");
	}

	public get depsFolders(): string[] {
		return this.get("depsFolders") ?? [];
	}

	public get snippets(): { name: string; command: string }[] {
		return this.get("snippets", []);
	}

	public get updateEnabled(): boolean {
		return this.get("selfUpdateEnabled", true);
	}

	public get remoteBase(): string | undefined {
		return this.get("remoteBase");
	}
	public get useRemoteBase(): boolean {
		return this.get("useRemoteBase", true);
	}

	//#region guessed settings
	public guessCASBin(): string | undefined {
		try {
			return normalize(
				execSync(`which cas`, {
					// CAS_DIR is used in CAS documentation, so it may be set, but not be in PATH
					env: {
						...process.env,
						PATH: `${process.env.CAS_DIR}:${process.env.PATH ?? ""}`,
					},
				})
					.toString()
					.trim(),
			);
		} catch {
			return undefined;
		}
	}

	public guessCASServerBin(): string | undefined {
		try {
			return normalize(
				execSync(`which cas_server.py`, {
					// CAS_DIR is used in CAS documentation, so it may be set, but not be in PATH
					env: {
						...process.env,
						PATH: `${process.env.CAS_DIR}:${process.env.PATH ?? ""}`,
					},
				})
					.toString()
					.trim(),
			);
		} catch {
			return undefined;
		}
	}

	static getWorkspaceDir(): string {
		if (vscode.workspace.workspaceFile) {
			return dirname(vscode.workspace.workspaceFile.fsPath);
		} else if (
			vscode.workspace.workspaceFolders &&
			vscode.workspace.workspaceFolders.length > 0
		) {
			return vscode.workspace.workspaceFolders[0].uri.fsPath;
		} else {
			return realpathSync(".");
		}
	}

	//#region pickers
	public async pickCasServer() {
		const ccl = await vscode.window
			.showOpenDialog({
				canSelectFiles: true,
				canSelectMany: false,
				canSelectFolders: false,
				title: "Select cas server script ...",
			})
			.then((data) => {
				return data?.pop()?.fsPath;
			});
		if (this.casServer) {
			await this.set("server", ccl);
		} else {
			await this.set("server", ccl, vscode.ConfigurationTarget.Global);
		}

		debug(`[cas.Settings] CAS server selected ${this.casServer}`);
	}
	//#endregion
}
