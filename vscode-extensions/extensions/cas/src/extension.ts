import { setupApi } from "@cas/http";
import { setupLogger, vscodeSink } from "@cas/logs";
import { getOtelSink, setupTelemetry } from "@cas/telemetry";
import { getLogger } from "@logtape/logtape";
import updaterPackageJSON from "cas-updater/package.json" with { type: "json" };
import { gt } from "semver";
import * as vscode from "vscode";
import { Builder } from "./cas_tools/builder";
import { DBProvider } from "./db/index";
import { registerEnrichment } from "./files";
import { InlineMacrosProvider } from "./inline_macros";
import { OpenGrokApi } from "./og";
import { Settings } from "./settings";
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
	const startTime = Date.now();

	// Initialize settings first (needed for logger config)
	const settings = new Settings(context);
	await settings.initSettings();

	// Setup telemetry and logging
	const telemetry = setupTelemetry(context, settings.telemetryProvider, {
		endpoint: settings.telemetryHost,
	});

	await setupLogger("CAS", {
		sinks: {
			telemetry: getOtelSink(),
		},
		additionalSinks: {
			vscodeServer: vscodeSink("CAS Server"),
		},
		filters: {},
		lowestLevel: "debug",
		additionalLoggers: [
			{
				category: ["CAS", "Server"],
				parentSinks: "override",
				sinks: ["console", "vscodeServer"],
			},
		],
	});

	const logger = getLogger(["CAS", "extension"]);
	logger.debug`Starting cas extension ...`;

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

	// Track activation
	telemetry.logUsage("extension.activate.start", {
		environment: process.env.NODE_ENV || "development",
		remoteName: vscode.env.remoteName || "local",
		extensionVersion: context.extension.packageJSON.version,
		telemetryEnabled: settings.telemetryEnabled,
		telemetryProvider: settings.telemetryProvider,
		remoteBaseEnabled: settings.useRemoteBase,
	});

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
			logger.warn`Failed to install cas-updater extension`;
		}
	}
	if (
		vscode.env.remoteName &&
		context.extension.extensionKind === vscode.ExtensionKind.UI
	) {
		logger.debug`running in remote as a UI extension - ending early`;
		return;
	}
	//#endregion

	telemetry.logUsage("extension.init", {
		activationTime: Date.now() - startTime,
		remoteConnection: vscode.env.remoteName || "none",
		workspaceFolders: vscode.workspace.workspaceFolders?.length || 0,
		settingsLoaded: true,
	});
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

	logger.info`CAS extension activated`;
	telemetry.logUsage("extension.activate.complete", {
		totalTime: Date.now() - startTime,
		componentsInitialized: [
			"dbProvider",
			"manifestSettings",
			"remoteConnectionManager",
			"workspaceGenerator",
			"views",
		],
	});
}
