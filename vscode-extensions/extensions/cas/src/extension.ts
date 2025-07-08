import { setupApi } from "@cas/http";
import updaterPackageJSON from "cas-updater/package.json" with { type: "json" };
import { gt, lt, lte } from "semver";
import * as vscode from "vscode";
import { Builder } from "./cas_tools/builder";
import { DBProvider } from "./db/index";
import { registerEnrichment } from "./files";
import { InlineMacrosProvider } from "./inline_macros";
import { debug, info, warn } from "./logger";
import { OpenGrokApi } from "./og";
import { Settings } from "./settings";
import { createLogger } from "./telemetry";
import { CASCmdView } from "./views/view_cas_cmd";
import { CASToolboxView } from "./views/view_cas_toolbox";
import { CASCompilationInfo } from "./views/view_compilation_info";
import { DepsTree } from "./views/view_depstree";
import { CASFileInfoView } from "./views/view_file_info";
import { FlamegraphView } from "./views/view_flamegraph";
import { OGView } from "./views/view_og";
import { CASProcTreeView } from "./views/view_proctree";
import { WorkspaceGenerator } from "./workspaces/generator";
import { ManifestSettings } from "./workspaces/manifest";
import { RemoteConnctionManager } from "./workspaces/remote";

export async function activate(context: vscode.ExtensionContext) {
	debug("[cas.activate] Starting cas extension ...");
	await vscode.commands.executeCommand(
		"setContext",
		"cas.development",
		process.env.NODE_ENV !== "production",
	);
	// trivial API proposal gating bypass :)
	context.extension.packageJSON.enabledApiProposals = [
		"resolvers",
		"portsAttributes",
		"timeline",
	];

	info("creating settings");
	const settings = new Settings(context);
	await settings.initSettings();

	// prepare HTTP client
	await setupApi(context);

	if (
		!vscode.extensions.getExtension("Samsung.cas-updater") ||
		gt(
			updaterPackageJSON.version,
			vscode.extensions.getExtension("Samsung.cas-updater")?.packageJSON
				.version,
		)
	) {
		try {
			await vscode.commands.executeCommand(
				"workbench.extensions.installExtension",
				vscode.Uri.joinPath(context.extensionUri, "dist/updater.vsix"),
			);
		} catch {
			warn("Failed to install cas-updater extension");
		}
	}
	if (
		vscode.env.remoteName &&
		context.extension.extensionKind === vscode.ExtensionKind.UI
	) {
		debug("running in remote as a UI extension - ending early");
		return;
	}
	//#endregion

	const telemetry = createLogger(settings, context);
	telemetry.logUsage("init");
	const dbProvider = new DBProvider(context, settings);
	const manifestSettings = new ManifestSettings(context, settings, dbProvider);
	const _remoteConnectionManager = new RemoteConnctionManager(
		context,
		manifestSettings,
	);
	const wsGenerator = new WorkspaceGenerator(
		context,
		dbProvider,
		settings,
		manifestSettings,
	);
	const _depsTree = new DepsTree(
		context,
		dbProvider,
		wsGenerator,
		settings,
		manifestSettings,
	);
	const cmdView = new CASCmdView(context, dbProvider, wsGenerator, settings);
	const _procView = new CASProcTreeView(
		context,
		dbProvider,
		wsGenerator,
		settings,
	);
	const _provider = new CASToolboxView(
		context,
		cmdView,
		dbProvider,
		wsGenerator,
		settings,
	);
	const _info = new CASFileInfoView(
		context,
		cmdView,
		dbProvider,
		wsGenerator,
		settings,
	);
	const _compInfo = new CASCompilationInfo(context, dbProvider, settings);
	const og = new OpenGrokApi(context, settings, manifestSettings);
	const _ogView = new OGView(context, settings, og);
	const _flamegraphView = new FlamegraphView(context, settings, dbProvider);
	const _BuildVerifier = new Builder(context, settings, dbProvider);
	const _inlineMacros = new InlineMacrosProvider(context, settings);
	registerEnrichment(context, dbProvider);

	info("[cas.activate] CAS extension activated.");
}
