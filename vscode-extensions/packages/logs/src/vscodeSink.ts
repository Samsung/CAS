import { getTextFormatter, LogLevel, LogRecord, Sink } from "@logtape/logtape";
import { window } from "vscode";

const levelMap: Record<
	LogLevel,
	"trace" | "debug" | "info" | "warn" | "error"
> = {
	trace: "trace",
	debug: "debug",
	info: "info",
	warning: "warn",
	error: "error",
	fatal: "error",
} as const;

export function vscodeSink(name: string): Sink & Disposable {
	const channel = window.createOutputChannel(name, {
		log: true,
	});

	const sink: Sink & Disposable = (record: LogRecord) => {
		channel[levelMap[record.level]](
			getTextFormatter({
				category: ".",
				timestamp: "none",
				format: ({ timestamp, level, category, message }) =>
					`[${category}]: ${message}`,
			})(record),
		);
	};
	sink[Symbol.dispose] = channel.dispose;

	return sink;
}
