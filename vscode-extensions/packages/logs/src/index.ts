import {
	configure,
	FilterLike,
	getConsoleSink,
	LoggerConfig,
	LogLevel,
	Sink,
} from "@logtape/logtape";
import { vscodeSink } from "./vscodeSink";

export { vscodeSink } from "./vscodeSink";

export interface LoggerOptions {
	sinks?: Record<string, Sink>;
	additionalSinks?: Record<string, Sink>;
	filters?: Record<string, FilterLike>;
	lowestLevel?: LogLevel;
	additionalLoggers?: LoggerConfig<string, string>[];
}

export async function setupLogger(
	applicationName: string,
	options: LoggerOptions = {},
) {
	await configure({
		sinks: {
			console: getConsoleSink(),
			vscode: vscodeSink(applicationName),
			...(options.sinks ?? {}),
			...(options.additionalSinks ?? {}),
		},
		filters: options.filters,
		loggers: [
			{
				category: applicationName,
				lowestLevel: options.lowestLevel ?? "debug",
				sinks: ["console", "vscode", ...Object.keys(options.sinks ?? {})],
			},
			...(options.additionalLoggers ?? []),
		],
	});
}
