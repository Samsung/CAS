import type { ArchiveSource, P4MappingType } from "@cas/manifest";
import { workspace } from "vscode";

class Settings {
	//#region properties
	readonly id = "cas-p4";
	readonly configuration;

	//#endregion
	//#region initialization
	constructor() {
		this.configuration = workspace.getConfiguration();
		return new Proxy(this, {
			get(target, p: keyof Settings | string, receiver) {
				if (p in target || typeof p === "symbol") {
					return Reflect.get(target, p, receiver);
				}
				return target.get(p);
			},
			set(target, p: keyof Settings | string, newValue, receiver) {
				if (p in target && Object.getOwnPropertyDescriptor(target, p)?.set) {
					return Reflect.set(target, p, newValue, receiver);
				}
				target.set(p, newValue);
				return true;
			},
		});
	}
	//#endregion

	//#region setting getters
	get mappingTemplatePath(): string | undefined {
		return this.get<string>("mappingTemplatePath");
	}
	set mappingTemplatePath(path: string) {
		this.set<string>("mappingTemplatePath", path);
	}

	get p4Path(): string {
		return this.get<string>("p4Path", "p4");
	}
	set p4Path(path: string) {
		this.set<string>("p4Path", path);
	}

	get maxP4Tries(): number {
		return this.get<number>("maxP4Tries", 5);
	}

	get maxDepsSearchAttempts(): number {
		return 10; // this doesn't have to be configurable probably
	}

	get buildMappings(): P4MappingType[] {
		return this.get<P4MappingType[]>("mappings", []);
	}
	get p4server(): string | undefined {
		return this.get<string>("p4Server");
	}

	get artifactoryURL(): ArchiveSource | undefined {
		return this.get<string>("artifactoryLink");
	}

	get archiveURL(): string | undefined {
		return this.get<string>("intermediateArchive");
	}

	get cleanListURL(): string | undefined {
		return this.get<string>("cleanSourceList");
	}

	get isP4Workspace(): boolean {
		return (
			this.get<string>("type") === "p4" ||
			!this.configuration.has("cas.manifest")
		);
	}

	watch<T>(setting: string, listener: (value: T | undefined) => unknown) {
		workspace.onDidChangeConfiguration((event) => {
			if (
				event.affectsConfiguration(`${this.id}.${setting}`) ||
				event.affectsConfiguration(`cas.manifest.sourceRepo.${setting}`)
			) {
				listener(this.get<T>(setting));
			}
		});
	}

	//#endregion

	//#region general functions
	public set<T>(key: string, value: T) {
		return this.configuration.update(`${this.id}.${key}`, value);
	}

	public get<T>(key: string): T | undefined;
	public get<T>(key: string, defaultValue: T): T;
	public get<T>(key: string, defaultValue?: T) {
		// prioritize manifest
		if (this.configuration.has(`cas.manifest.sourceRepo.${key}`)) {
			return this.configuration.get<T>(`cas.manifest.sourceRepo.${key}`);
		}
		if (typeof defaultValue !== "undefined") {
			return this.configuration.get<T>(`${this.id}.${key}`, defaultValue);
		}
		return this.configuration.get<T>(`${this.id}.${key}`);
	}
	public has(key: string): boolean {
		return this.configuration.has(key);
	}
	//#endregion
}

const settings = new Settings();

export default settings;
