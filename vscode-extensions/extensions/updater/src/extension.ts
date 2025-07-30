import { readdir } from "node:fs/promises";
import { dirname } from "node:path";
import { sleep } from "@cas/helpers/promise.js";
import { setupLogger } from "@cas/logs";
import { getLogger } from "@logtape/logtape";
import { gt } from "semver";
import {
	commands,
	ExecServer,
	ExtensionContext,
	ExtensionMode,
	env,
	extensions,
	Uri,
	workspace,
} from "vscode";
import { ExtensionInstaller } from "./install";

const casExtNameRegex = /samsung.cas-(?<version>\d+\.\d+\.\d+)/i;

export async function activate(context: ExtensionContext) {
	context.extension.packageJSON.enabledApiProposals = [
		"resolvers",
		"extensionsAny",
	];
	let execServer: ExecServer | undefined;
	extensions.onDidChange(checkAndUninstall);
	setImmediate(checkAndUninstall);

	if (
		// auto-update is a bad idea in dev and doesn't really work properly since extensions aren't really packaged there
		context.extensionMode !== ExtensionMode.Development &&
		// ensure auto-update is enabled
		workspace
			.getConfiguration()
			.get<boolean>("cas.selfUpdateEnabled", true) &&
		// ensure we're connected to a remote host
		env.remoteAuthority &&
		env.remoteName &&
		(execServer = await workspace.getRemoteExecServer(env.remoteAuthority))
	) {
		const extName =
			context.extension.packageJSON.displayName ?? context.extension.id;
		await setupLogger(extName);
		const logger = getLogger(["Updater"]);

		logger.info`started update checks`;

		const casDirName = (await readdir(dirname(context.extensionPath)))
			.filter((name) => casExtNameRegex.test(name))
			?.reduce(
				(current, max) =>
					gt(
						current.match(casExtNameRegex)?.groups?.version ?? "0.0.0",
						max.match(casExtNameRegex)?.groups?.version ?? "0.0.0",
					)
						? current
						: max,
				"",
			);
		const localVersion = casDirName.match(casExtNameRegex)?.groups?.version;
		if (!casDirName || !localVersion) {
			logger.error`CAS extension not found locally! If this was intentional, please uninstall CAS Updater extension as well`;
			return;
		}
		const existingExtension = extensions.getExtension("Samsung.cas", true);
		logger.debug(
			`exists: ${!!existingExtension} version: ${localVersion} (local) vs ${existingExtension?.packageJSON?.version} (remote)`,
		);
		// ensure we're not overwriting a lower or equal version of CAS extension
		const installer = new ExtensionInstaller(
			context,
			Uri.joinPath(context.extensionUri, `../${casDirName}`),
			localVersion,
			execServer,
		);
		if (
			!existingExtension ||
			gt(localVersion, existingExtension.packageJSON.version)
		) {
			logger.info(`Installing new version of CAS extension: v${localVersion}`);
			try {
				const success = await installer.installExtension();
				if (!success) {
					logger.error`Failed installing extension`;
				}
			} catch (e) {
				logger.error`error installing extension: ${e}`;
			}
		} else {
			logger.info(
				`Latest known version of CAS extension already installed: v${existingExtension.packageJSON.version}`,
			);
		}
		return installer;
	}
}

async function checkAndUninstall() {
	if (
		!extensions.getExtension("Samsung.cas") &&
		// ensure it really is gone not just temporarily missing for e.g. update
		!(await sleep(1000).then(() => extensions.getExtension("Samsung.cas")))
	) {
		await commands.executeCommand(
			"workbench.extensions.uninstallExtension",
			"Samsung.cas-updater",
		);
	}
}
