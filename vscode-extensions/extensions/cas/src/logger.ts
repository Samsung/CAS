import * as vscode from "vscode";

export let outputChannel: vscode.LogOutputChannel | undefined;
export let serverOutputChannel: vscode.LogOutputChannel | undefined;

function initialize() {
	if (!outputChannel) {
		outputChannel = vscode.window.createOutputChannel("CAS", { log: true });
	}
}
function srvInitialize() {
	if (!serverOutputChannel) {
		serverOutputChannel = vscode.window.createOutputChannel("CAS-Server", {
			log: true,
		});
	}
}

const levelMap = {
	[vscode.LogLevel.Off]: null,
	[vscode.LogLevel.Trace]: "T",
	[vscode.LogLevel.Debug]: "D",
	[vscode.LogLevel.Info]: "I",
	[vscode.LogLevel.Warning]: "W",
	[vscode.LogLevel.Error]: "E",
};

function log(message: any, level: vscode.LogLevel, showPopup = false) {
	initialize();
	outputChannel?.[
		vscode.LogLevel[level].toLowerCase().replace("warning", "warn") as
			| "trace"
			| "info"
			| "debug"
			| "warn"
			| "error"
	]?.(`[${levelMap[level]}] ${String(message)}`);

	if (showPopup) {
		vscode.window.showErrorMessage(String(message));
	}
}

export function trace(message: any, showPopup = false) {
	log(message, vscode.LogLevel.Trace, showPopup);
}

export function debug(message: any, showPopup = false) {
	log(message, vscode.LogLevel.Debug, showPopup);
}

export function info(message: any, showPopup = false) {
	log(message, vscode.LogLevel.Info, showPopup);
}

export function warn(message: any, showPopup = false) {
	log(message, vscode.LogLevel.Warning, showPopup);
}

export function error(message: any, showPopup = false) {
	log(message, vscode.LogLevel.Error, showPopup);
}

export function srvLog(message: any) {
	srvInitialize();
	serverOutputChannel?.appendLine(`${String(message)}`);
}
