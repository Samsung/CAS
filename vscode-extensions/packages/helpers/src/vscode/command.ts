import { Terminal, TerminalOptions, window } from "vscode";
import { withAbort } from "../promise";

type ExitCode = number;

export interface CommandOptions extends TerminalOptions {
	onOutput?: (output: string) => unknown;
	processOutputStream?: (output: AsyncIterable<string>) => unknown;
	onExit?: (exitCode: ExitCode | undefined) => unknown;
}
type KillCommand = (
	signal?: "SIGINT" | "SIGHUP" | "SIGKILL" | number,
) => Promise<ExitCode | undefined>;

export interface CommandResult {
	terminal: Terminal;
	started: Promise<void>;
	finished: Promise<ExitCode | undefined>;
	kill: KillCommand;
}

export function runCommand(
	cmd: string,
	args?: string[],
	options: CommandOptions = {
		hideFromUser: true,
		isTransient: true,
	},
): CommandResult {
	options.hideFromUser ??= true;
	options.isTransient ??= true;
	const terminalRunner = window.createTerminal(options);
	const { promise: hasCommandStarted, resolve: commandStarted } =
		Promise.withResolvers<void>();
	const { promise: hasCommandFinished, resolve: commandFinished } =
		Promise.withResolvers<ExitCode | undefined>();
	let kill: KillCommand = async (signal) => {
		const pid = await terminalRunner.processId;
		if (pid) {
			process.kill(pid, signal || "SIGINT");
		}
		return withAbort(hasCommandFinished, AbortSignal.timeout(200));
	};
	window.onDidChangeTerminalShellIntegration(
		async ({ terminal, shellIntegration }) => {
			if (terminal === terminalRunner) {
				const commandExecution = args?.length
					? shellIntegration.executeCommand(cmd, args)
					: shellIntegration.executeCommand(cmd);
				const outputStream = commandExecution.read();
				window.onDidStartTerminalShellExecution(({ execution }) => {
					if (execution === commandExecution) {
						commandStarted();
					}
				});
				window.onDidEndTerminalShellExecution(({ execution, exitCode }) => {
					if (execution === commandExecution) {
						commandFinished(exitCode);
						options.onExit && options.onExit(exitCode);
					}
				});

				options.processOutputStream &&
					(await options.processOutputStream(outputStream));
				if (options.onOutput) {
					for await (const data of outputStream) {
						options.onOutput(data);
					}
				}
			}
		},
	);
	return {
		terminal: terminalRunner,
		started: hasCommandStarted,
		finished: hasCommandFinished,
		kill,
	};
}
