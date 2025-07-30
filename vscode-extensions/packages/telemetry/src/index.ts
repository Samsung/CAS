import { chunk } from "@cas/helpers";
import { getOpenTelemetrySink } from "@logtape/otel";
import {
	type Attributes,
	context,
	diag,
	propagation,
	SpanKind,
	SpanStatusCode,
	trace,
} from "@opentelemetry/api";
import { getNodeAutoInstrumentations } from "@opentelemetry/auto-instrumentations-node";
import { OTLPLogExporter } from "@opentelemetry/exporter-logs-otlp-http";
import { OTLPMetricExporter } from "@opentelemetry/exporter-metrics-otlp-http";
import { PrometheusExporter } from "@opentelemetry/exporter-prometheus";
import { OTLPTraceExporter } from "@opentelemetry/exporter-trace-otlp-http";
import {
	BatchLogRecordProcessor,
	LoggerProvider,
	LogRecordProcessor,
} from "@opentelemetry/sdk-logs";
import {
	MetricReader,
	PeriodicExportingMetricReader,
} from "@opentelemetry/sdk-metrics";
import { NodeSDK } from "@opentelemetry/sdk-node";
import {
	ConsoleSpanExporter,
	SpanExporter,
} from "@opentelemetry/sdk-trace-node";
import {
	Disposable,
	EventEmitter,
	type ExtensionContext,
	env,
	type TelemetryLogger,
	type TelemetrySender,
	type TelemetryTrustedValue,
	env as vscenv,
	workspace,
} from "vscode";

interface TelemetryConfig {
	traceExporter?: SpanExporter;
	metricReader?: MetricReader;
	logProcessor?: LogRecordProcessor;
}

class OpenTelemetrySender implements TelemetrySender, Disposable {
	private sdk: NodeSDK;
	private disabledAbort = new AbortController();

	public readonly onChangeEnableSate = new EventEmitter<boolean>().event;
	public config: TelemetryConfig;
	constructor(enabled: boolean, config: TelemetryConfig = {}) {
		this.config = config;
		this.sdk = new NodeSDK({
			traceExporter: config.traceExporter,
			metricReader: config.metricReader,
			instrumentations: [
				getNodeAutoInstrumentations({
					"@opentelemetry/instrumentation-undici": {
						ignoreRequestHook: (req) => {
							return (
								typeof req.headers === "string" ||
								new Headers([...chunk(req.headers, 2)] as [string, string][])
									.get("User-Agent")
									?.toLowerCase() !== "cas/extension"
							);
						},
					},
					"@opentelemetry/instrumentation-http": {
						enabled: false,
					},
					"@opentelemetry/instrumentation-net": {
						enabled: false,
					},
				}),
			],
			autoDetectResources: true,
			serviceName: "cas",
			logRecordProcessors:
				config.logProcessor !== undefined ? [config.logProcessor] : undefined,
		});

		this.toggleEnabled(enabled);
		this.sdk.start();
	}
	async dispose() {
		await this.flush();
	}
	#enabled: boolean = true;
	get enabled() {
		return this.#enabled;
	}

	toggleEnabled(enabled: boolean | undefined) {
		if (enabled) {
			this.#enabled = true;
			this.disabledAbort = new AbortController();
			// @ts-ignore
			if (this.sdk._disabled) {
				this.sdk.start();
			}
		} else {
			this.#enabled = false;
			this.disabledAbort.abort();
			this.sdk.shutdown();
		}
	}

	sendEventData(
		eventName: string,
		data?: Record<string, any>,
		parentContext?: Record<string, any>,
	): void {
		const tracer = trace.getTracer("vscode-extension");
		const startTime = Date.now();

		const spanContext = parentContext
			? propagation.extract(context.active(), parentContext)
			: context.active();

		tracer.startActiveSpan(
			eventName,
			{
				kind: SpanKind.INTERNAL,
				attributes: {
					...this.getStandardAttributes(),
					...(data ? this.sanitizeAttributes(data) : {}),
				},
			},
			spanContext,
			(span) => {
				span.setAttribute("startTime", startTime);
				diag.debug(eventName);

				try {
					span.end();
					span.setAttribute("duration", Date.now() - startTime);
				} catch {}
			},
		);
	}

	sendErrorData(
		error: Error,
		data?: Record<string, any>,
		parentContext?: Record<string, any>,
	): void {
		const tracer = trace.getTracer("vscode-extension");
		const startTime = Date.now();

		const spanContext = parentContext
			? propagation.extract(context.active(), parentContext)
			: undefined;

		const span = tracer.startSpan(
			"error",
			{
				kind: SpanKind.INTERNAL,
				attributes: {
					...this.getStandardAttributes(),
					...(data ? this.sanitizeAttributes(data) : {}),
					startTime,
				},
			},
			spanContext,
		);

		span.recordException(error);
		span.setStatus({ code: SpanStatusCode.ERROR, message: error.message });

		try {
			span.end();
			span.setAttribute("duration", Date.now() - startTime);
		} catch {}
	}

	async flush(): Promise<void> {
		await this.sdk.shutdown();
	}

	private getStandardAttributes(): Attributes {
		return {
			"service.name": "cas",
			"service.version": process.env.npm_package_version || "unknown",
			"telemetry.sdk.name": "opentelemetry",
			"telemetry.sdk.language": "typescript",
		};
	}

	private sanitizeAttributes(data: Record<string, any>): Attributes {
		const attrs: Attributes = {};
		for (const [key, value] of Object.entries(data)) {
			if (
				typeof value === "string" ||
				typeof value === "number" ||
				typeof value === "boolean"
			) {
				attrs[key] = value;
			} else if (value !== null && value !== undefined) {
				attrs[key] = String(value);
			}
		}
		return attrs;
	}
}

export enum ExporterType {
	Console = "console",
	Prometheus = "prometheus",
	OTLP = "otlp",
}

interface TelemetryConfig {
	endpoint?: string;
	port?: number;
}
const telemetryTypes = ["metrics", "traces", "logs"] as const;

type MultiTypeTelemetryConfig = {
	[key in (typeof telemetryTypes)[number]]: TelemetryConfig;
};

function getConfigForType(
	config: MultiTypeTelemetryConfig | TelemetryConfig,
	type: (typeof telemetryTypes)[number],
): TelemetryConfig {
	return type in config
		? (config as MultiTypeTelemetryConfig)[type]
		: (config as TelemetryConfig);
}

let sender: OpenTelemetrySender;
function createTelemetrySender(
	exporterType: ExporterType,
	config: TelemetryConfig | MultiTypeTelemetryConfig = {},
): OpenTelemetrySender {
	let traceExporter: SpanExporter | undefined;
	let metricReader: MetricReader | undefined;
	let logProcessor: LogRecordProcessor | undefined;

	switch (exporterType) {
		case ExporterType.Prometheus:
			metricReader = new PrometheusExporter({
				endpoint: getConfigForType(config, "metrics").endpoint,
				port: getConfigForType(config, "metrics").port || 9464,
			});
			traceExporter = new ConsoleSpanExporter();
			break;
		// @ts-ignore intentional fallthrough
		case ExporterType.OTLP:
			try {
				if (!getConfigForType(config, "metrics").endpoint) {
					throw new Error("OTLP exporter requires an endpoint");
				}
				traceExporter = new OTLPTraceExporter({
					url: new URL(
						"v1/traces",
						getConfigForType(config, "traces").endpoint,
					).toString(),
				});
				metricReader = new PeriodicExportingMetricReader({
					exporter: new OTLPMetricExporter({
						url: new URL(
							"v1/metrics",
							getConfigForType(config, "metrics").endpoint,
						).toString(),
					}),
				});
				logProcessor = new BatchLogRecordProcessor(
					new OTLPLogExporter({
						url: new URL(
							"v1/logs",
							getConfigForType(config, "logs").endpoint,
						).toString(),
					}),
				);
				return new OpenTelemetrySender(
					true, // enabled by default
					{ traceExporter, metricReader, logProcessor },
				);
			} catch {}
		case ExporterType.Console:
		default:
			traceExporter = new ConsoleSpanExporter();
	}

	return new OpenTelemetrySender(
		true, // enabled by default
		{ traceExporter, metricReader, logProcessor },
	);
}
export interface CasTelemetryLogger {
	logUsage(
		eventName: string,
		data?: Record<string, any | TelemetryTrustedValue>,
	): void;
	logError(
		eventName: Error,
		data?: Record<string, any | TelemetryTrustedValue>,
	): void;
}
let logger: TelemetryLogger;
let host: string | undefined;
export function setupTelemetry(
	context: ExtensionContext,
	type: ExporterType,
	config: TelemetryConfig | MultiTypeTelemetryConfig = {},
) {
	sender = createTelemetrySender(type, config);
	logger = vscenv.createTelemetryLogger(sender);
	vscenv.onDidChangeTelemetryEnabled(sender.toggleEnabled, sender);
	host = getConfigForType(config, "traces").endpoint;
	workspace.onDidChangeConfiguration((e) => {
		if (e.affectsConfiguration("telemetry.telemetryLevel")) {
			sender.toggleEnabled(
				workspace.getConfiguration().get("telemetry.telemetryLevel") !==
					"off" && env.isTelemetryEnabled,
			);
		}
	});
	context.subscriptions.push(sender, logger);
	return logger;
}

export function getTelemetryLoggerFor(name: string): CasTelemetryLogger {
	return {
		logUsage(
			eventName: string,
			data?: Record<string, any | TelemetryTrustedValue>,
		): void {
			return logger.logUsage(eventName, { source: name, ...data });
		},
		logError(
			eventName: Error,
			data?: Record<string, any | TelemetryTrustedValue>,
		): void {
			return logger.logError(eventName, { source: name, ...data });
		},
	};
}
export function sendTelemetryLog(
	message: string,
	level: "info" | "debug" | "warn" | "error",
) {
	diag[level](message);
}

export function isTelemetryEnabled() {
	return logger.isUsageEnabled && sender.enabled;
}
export function getTelemetryHost() {
	return host;
}

export function onTelemetryEnabledStateChange(
	func: (enabled: boolean) => void,
) {
	sender.onChangeEnableSate(func);
}

export function getOtelSink() {
	return getOpenTelemetrySink({
		loggerProvider: sender.config.logProcessor
			? new LoggerProvider({
					processors: [sender.config.logProcessor],
				})
			: undefined,
	});
}
