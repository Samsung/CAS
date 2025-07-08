/**
 * @module workspaces/remote
 *
 * Handles connections to a remote workspaces
 */

import { homedir } from "node:os";
import { normalize } from "node:path";
import { dirname, extname } from "node:path/posix";
import {
	decodeText,
	encodeText,
	withLeadingSlash,
	withTrailingSlash,
} from "@cas/helpers";
import { Manifest } from "@cas/manifest";
import { encode as encodeBase64 } from "@protobufjs/base64";
import { parse as sshParse, stringify } from "ssh-config";
import { commands, ExtensionContext, env, Uri, workspace } from "vscode";
import { debug, info } from "../logger";
import { ManifestSettings } from "./manifest";

interface HostInfo {
	hostname: string;
	username?: string;
	port?: number;
	keyFile?: string;
}
export class RemoteConnctionManager {
	#ctx: ExtensionContext;
	#manifest: ManifestSettings;
	constructor(ctx: ExtensionContext, manifest: ManifestSettings) {
		this.#ctx = ctx;
		this.#manifest = manifest;
		this.#ctx.subscriptions.push(
			this.#manifest.onParsedManifest(async (parsed) => {
				if (parsed?.remote) {
					return this.connect(parsed.remote);
				}
			}),
			this.#manifest.onInitDone(async () => this.checkSSHState()),
		);
	}

	get configUri() {
		return Uri.file(
			workspace.getConfiguration().get("remote.SSH.configFile") ||
				`${homedir()}/.ssh/config`,
		);
	}

	/**
	 * Connect to a remote SSH workspace
	 * @param target ssh URI to connect to (ssh://user@host:port/path/to/folder)
	 * @returns technically returtns the result of openFolder command,
	 * however as it causes VSCode to reopen the window the promise value can't really be used.
	 * The promise itself could be used to track if the request is still processing, I guess.
	 */
	public async connect({ hostname, username, port, keyFile }: HostInfo) {
		await this.configureHost({ hostname, username, port, keyFile });
		const targetWorkspace = withLeadingSlash(await this.getTargetFolder());
		// transfer manifest to remote via global state
		await this.#ctx.globalState.update(
			`ssh-state-ssh-remote+${await this.toAuthorityString({ hostname, username, port }, this.#manifest.manifest)}`,
			// strip info about the remote to avoid multiple reloads
			{ ...this.#manifest.manifest, remote: undefined },
		);
		const vscodeUri = Uri.parse(
			`vscode-remote://ssh-remote+${await this.toAuthorityString({ hostname, username, port }, this.#manifest.manifest)}${targetWorkspace}`,
		);
		return commands.executeCommand("vscode.openFolder", vscodeUri);
	}

	private async toAuthorityString(
		{ hostname, username, port }: HostInfo,
		manifest?: Manifest,
	) {
		if (
			username ||
			port ||
			hostname.toLowerCase() !== hostname ||
			hostname.match(/[\/\\\+]/)
		) {
			const manifestHash = await crypto.subtle.digest(
				"SHA-256",
				new TextEncoder().encode(JSON.stringify(manifest)),
			);
			return Buffer.from(
				JSON.stringify({
					hostName: hostname,
					user: username,
					port,
					digest: encodeBase64(
						new Uint8Array(manifestHash),
						0,
						manifestHash.byteLength,
					),
				}),
			).toString("hex");
		}
		return hostname;
	}

	// alternative if folder path is not defined - use sourceRepo
	private async getTargetFolder() {
		if (workspace.workspaceFile) {
			const workspaceData = JSON.parse(
				decodeText(await workspace.fs.readFile(workspace.workspaceFile)),
			);
			if (
				"folders" in workspaceData &&
				Array.isArray(workspaceData.folders) &&
				workspaceData.folders.length &&
				// don't just try current path hoping it will work
				workspaceData.folders.at(0).path !== "."
			) {
				return workspaceData.folders.at(0).path; // TODO: handle multiple folders? or just use the first one?
			}
		}
		switch (this.#manifest.manifest?.sourceRepo.type) {
			case "local": {
				return this.#manifest.manifest.sourceRepo.sourceRoot;
			}
			default: {
				if (this.#manifest.manifest?.basProvider?.type === "file") {
					return dirname(this.#manifest.manifest?.basProvider.path);
				}
				return workspace.workspaceFolders?.at(0)?.uri.fsPath ?? "";
			}
		}
	}

	/**
	 * Ensure the host is configured as requested. Note that the remote SSH plugin only respects the SSH config files.
	 * @param target  ssh URI to connect to (ssh://user@host:port/path/to/folder)
	 */
	private async configureHost({
		hostname,
		username,
		port,
		keyFile,
	}: {
		hostname: string;
		username?: string;
		port?: number;
		keyFile?: string;
	}) {
		const sshConfigFile = await workspace.fs.readFile(this.configUri);
		const sshConfig = sshParse(decodeText(sshConfigFile));
		let currentHostSettings = sshConfig.compute({
			Host: hostname,
			User: username,
		});
		const newConfig: Record<string, string | string[]> = {};
		if (
			username &&
			(currentHostSettings.User !== username ||
				// if only user is set it means there wasn't an actual config found
				// it's an artifat of the `compute` function when used with a given User
				Object.keys(currentHostSettings)?.at(0) === "User")
		) {
			newConfig.User = username;
		}
		if (port && currentHostSettings.Port !== port.toString()) {
			newConfig.Port = port.toString();
		}
		if (keyFile && currentHostSettings.IdentityFile !== keyFile) {
			newConfig.IdentityFile = keyFile;
		}
		if (Object.keys(newConfig).length) {
			sshConfig.prepend(
				{ Host: hostname, HostName: hostname, ...newConfig },
				true,
			);
			await workspace.fs.writeFile(
				this.configUri,
				encodeText(stringify(sshConfig)),
			);
		}
	}

	/**
	 * Check for SSH workspace state setting and configure the workspace if necessary
	 */
	public async checkSSHState() {
		debug(
			`remote name: ${env.remoteName} remote authority: ${env.remoteAuthority}`,
		);
		if (!env.remoteName) {
			debug("[cas.remote] workspace is local - skipping remote data check");
			return;
		}
		for (const folder of workspace.workspaceFolders ?? []) {
			const state = this.#ctx.globalState.get<Manifest | undefined>(
				`ssh-state-${env.remoteAuthority}`,
			);
			if (!state || !Object.keys(state).length) {
				continue;
			}
			info(
				`[cas.remote] Loaded SSH workspace for the first time for ${folder.name} (at ${folder.uri.toString()})`,
			);
			debug(`[cas.remote] Loaded manifest: ${JSON.stringify(state, null, 2)}`);
			this.#manifest.parseManifestObject(state);

			// move state to workspace-local
			await this.#ctx.workspaceState.update(
				`ssh-state.${env.remoteName}.${env.remoteAuthority}`,
				state,
			);
			await this.#ctx.globalState.update(
				`ssh-state-${env.remoteAuthority}`,
				undefined,
			);
			await this.#manifest.setSettings(this.#manifest.manifest, true);
			// end early after setting manifest
			return;
		}
		const localState = this.#ctx.workspaceState.get(
			`ssh-state.${env.remoteName}.${env.remoteAuthority}`,
		);
		debug(
			`[cas.remote] Loaded local manifest: ${JSON.stringify(localState, null, 2)}`,
		);
		if (localState && Object.keys(localState).length) {
			info("[cas.remote] Loaded SSH workspace from local state");
			this.#manifest.parseManifestObject(localState);
		}
		debug("[cas.remote] No SSH state found");
	}
	private normalizePath(path: string) {
		return withLeadingSlash(
			withTrailingSlash(normalize(path).replaceAll("\\", "/")),
		);
	}
}
