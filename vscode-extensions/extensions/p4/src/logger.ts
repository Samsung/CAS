import { format, type TransformableInfo } from "logform";
import inspect from "object-inspect";
import { LEVEL, MESSAGE, SPLAT } from "triple-beam";
import { type ExtensionContext, window } from "vscode";
import { createLogger, type Logger } from "winston";
import { LogOutputChannelTransport } from "winston-transport-vscode";

export type LogLevel = "error" | "warn" | "info" | "debug" | "trace" | "off";

let logger: Logger;

let maxLabelLength = 0;

export const quiet = false;

const formatFunc = format((info: TransformableInfo): TransformableInfo => {
	const { level, message, label, ...rest } = info as TransformableInfo & {
		label: string;
	};
	delete rest[LEVEL];
	delete rest[MESSAGE];
	delete rest[SPLAT];

	let labelIndent = " ";
	if (label?.length > maxLabelLength) {
		maxLabelLength = label.length;
	} else {
		labelIndent = " ".repeat(maxLabelLength - label.length + 1);
	}

	let msg = `(${label})${labelIndent}${info[MESSAGE] ?? message}`;
	if (Object.keys(rest).length > 0) {
		msg += ` ${inspect(rest)}`;
	}

	return {
		...info,
		[LEVEL]: info[LEVEL] ?? level,
		[MESSAGE]: msg,
	};
});

/**
 * Get a logger with a given label
 * @param label name of the module for log formatting
 * @returns a logger instance
 */
export function getLogger(label?: string): Logger {
	return logger.child({ label });
}

/**
 * Initialize a logger with using vscode's LogOutputChannel
 * @param context plugin's context
 */
export async function initLogger(context: ExtensionContext): Promise<void> {
	const extName =
		context.extension.packageJSON.displayName ?? context.extension.id;
	logger = createLogger({
		level: "trace", // log everything and let vscode filter instead
		levels: {
			...LogOutputChannelTransport.config.levels,
			off: 10,
		},
		format: format.combine(formatFunc(), format.padLevels(), format.splat()),
		transports: [
			new LogOutputChannelTransport({
				outputChannel: window.createOutputChannel(extName, {
					log: true,
				}),
			}),
		],
	});
}
