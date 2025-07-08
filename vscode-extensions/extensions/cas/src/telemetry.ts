import { mkdir, open, rename, stat, unlink } from "node:fs/promises";
import { Readable } from "node:stream";
import { ReadableStream } from "node:stream/web";
import { scheduler, setImmediate } from "node:timers/promises";
import { chain } from "stream-chain";
import { parser } from "stream-json/jsonl/Parser";
import { stringer } from "stream-json/jsonl/Stringer";
import {
	Event,
	EventEmitter,
	ExtensionContext,
	env,
	TelemetryLogger,
	TelemetrySender,
	TelemetryTrustedValue,
	Uri,
} from "vscode";
import { debug, trace } from "./logger";
import { Settings } from "./settings";

interface CasTelemetryEventData {
	url?: string;
	source?: string;
	"common.extname": string;
	"common.extversion": string;
	"common.vscodeversion": string;
	"common.product": string;
	[key: string]: string | number | boolean | undefined;
}

class PlausibleSender implements TelemetrySender {
	#host: string;
	#endpoint: URL | undefined;
	#disabledAbort: AbortController;
	#eventQueue: { eventName: string; data: CasTelemetryEventData }[];
	#flushPromise: Promise<unknown>;
	#failures = 0;
	#saveCounter = 0;
	readonly #onDidChangeEnabledState = new EventEmitter<boolean>();
	public onChangeEnableSate: Event<boolean> =
		this.#onDidChangeEnabledState.event;
	#context: ExtensionContext;
	#settings: Settings;
	constructor(
		hostUrl: string,
		enabled: boolean,
		settings: Settings,
		context: ExtensionContext,
	) {
		this.#host = hostUrl;
		this.#context = context;
		this.#settings = settings;
		this.#endpoint = URL.canParse(hostUrl)
			? new URL("/api/event", hostUrl)
			: undefined;
		this.#eventQueue = [];
		this.#disabledAbort = new AbortController();
		this.#flushPromise = Promise.resolve();
		this.toggleEnabled(enabled);
		debug("finised telemetry setup");
	}
	toggleEnabled(enabled: boolean | undefined) {
		this.#onDidChangeEnabledState.fire(enabled ?? false);
		if (enabled) {
			this.#disabledAbort = new AbortController();
			debug("telemetry enabled");
		} else {
			this.#disabledAbort.abort();
			debug("telemetry disabled");
		}
	}
	get enabled() {
		return !this.#disabledAbort.signal.aborted;
	}
	get host() {
		return this.#host;
	}
	sendEventData(eventName: string, data: CasTelemetryEventData): void {
		// don't send anything if telemetry is disabled
		if (this.#disabledAbort.signal.aborted) {
			return;
		}
		// batch telemetry requests
		this.#eventQueue.push({ eventName, data });
		// we only need to create the queue task once
		if (this.#eventQueue.length === 1) {
			// wait until previous batch was sent
			this.#flushPromise.then(() => {
				this.#flushPromise = new Promise((resolve) => {
					// add a task to next event loop, since we don't need to do this immediately
					setImmediate(undefined, { signal: this.#disabledAbort.signal }).then(
						() => {
							if (this.#disabledAbort.signal.aborted) {
								return resolve(0);
							}
							const reqData = this.#eventQueue.map(({ eventName, data }) => ({
								headers: {
									Referrer: `extension://${data["common.extname"]}/${data["common.extversion"]}`,
								},
								body: {
									domain: data["common.extname"].toLowerCase(),
									name: eventName ?? "pageview",
									url:
										data?.url ??
										`extension://telemetry/${data?.source ? data.source : eventName}`,
									props: data,
									referrer: `extension://${data["common.extname"]}/${data["common.extversion"]}`,
								},
							}));
							// clear queue
							this.#eventQueue.length = 0;
							// resolve our promise only after all requests in the batch finished processing
							Promise.allSettled(
								reqData
									.map(({ body, headers }) =>
										this.#endpoint
											? fetch(this.#endpoint, {
													method: "POST",
													headers: {
														"Content-Type": "application/json",
														...headers,
													},
													body: JSON.stringify(body),
													signal: AbortSignal.any([
														AbortSignal.timeout(500),
														this.#disabledAbort.signal,
													]),
												})
													.then((response) => {
														if (!response.ok) {
															trace(
																`Failed to send telemetry, status: ${response.status}`,
															);
															throw new Error("Failed to send telemetry");
														}
													})
													.then(scheduler.yield)
											: Promise.resolve(),
									)
									.concat([
										this.#context.storageUri?.fsPath
											? mkdir(this.#context.storageUri.fsPath, {
													recursive: true,
												}).then(async () => {
													const writeStream = (
														await open(
															Uri.joinPath(
																this.#context.storageUri!,
																"telemetry.json",
															).fsPath,
															"a+",
														)
													).createWriteStream();
													chain(
														[
															ReadableStream.from(
																reqData.map(({ body }) => ({
																	timestamp: Date.now(),
																	...body,
																})),
															),
															stringer(),
															writeStream,
														],
														{
															readableObjectMode: false,
															writableObjectMode: false,
														},
													);
													this.#saveCounter += 1;
												})
											: new Promise(() => {}),
									]),
							)
								.then(async (results) => {
									// yield the event loop, telemetry tasks can wait
									await scheduler.yield();
									// require at least 3/4 requests to succeed for the batch to be considered successful as a whole
									if (
										results.reduce(
											(acc, result) =>
												(acc +=
													+(result.status === "fulfilled") / results.length),
											0,
										) < 0.75
									) {
										this.#failures += 1;
										// exponential backoff
										await scheduler.wait(
											Math.max(10 * 8 ** this.#failures, 300_000),
											{
												signal: this.#disabledAbort.signal,
											},
										);
									} else {
										this.#failures = 0;
									}
								})
								.then(async () => {
									// clean at most every 10th telemetry batch
									if (this.#saveCounter < 10 || !this.#context.storageUri) {
										return;
									}
									// yield the event loop, telemetry tasks can wait
									await scheduler.yield();
									this.#saveCounter = 0;
									const stats = await stat(
										Uri.joinPath(this.#context.storageUri, "telemetry.json")
											.fsPath,
									);
									// clear old telemetry if file is above 1MB
									if (stats.blocks * stats.blksize > 1024 * 1024) {
										await rename(
											Uri.joinPath(this.#context.storageUri, "telemetry.json")
												.fsPath,
											Uri.joinPath(
												this.#context.storageUri,
												"telemetry.json.old",
											).fsPath,
										);
										chain(
											[
												(
													await open(
														Uri.joinPath(
															this.#context.storageUri,
															"telemetry.json.old",
														).fsPath,
														"r",
													)
												).readableWebStream(),
												parser(),
												(obj: { timestamp: number }) =>
													obj.timestamp > Date.now() - 1000 * 60 * 60 * 24 * 7
														? obj
														: null,
												stringer(),
												(
													await open(
														Uri.joinPath(
															this.#context.storageUri,
															"telemetry.json",
														).fsPath,
														"w",
													)
												).createWriteStream(),
											],
											//
											{
												readableObjectMode: false,
												writableObjectMode: false,
											},
										);
										await unlink(
											Uri.joinPath(
												this.#context.storageUri,
												"telemetry.json.old",
											).fsPath,
										);
									}
								})
								.then(scheduler.yield)
								.then(resolve);
						},
					);
				});
			});
		}
	}
	sendErrorData(error: Error, data: CasTelemetryEventData): void {
		this.sendEventData("error", {
			...data,
			name: error.name,
			message: error.message,
			stack: error.stack,
		});
	}
	async flush?(): Promise<void> {
		await this.#flushPromise;
	}
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
let sender: PlausibleSender;
export function createLogger(
	settings: Settings,
	context: ExtensionContext,
): TelemetryLogger {
	if (!logger && settings) {
		sender = new PlausibleSender(
			settings.telemetryHost,
			settings.telemetryEnabled,
			settings,
			context,
		);
		settings.listen("telemetryEnabled", sender.toggleEnabled);
		logger = env.createTelemetryLogger(sender);
		context.subscriptions.push(logger);
	}
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

export function isTelemetryEnabled() {
	return logger.isUsageEnabled && sender.enabled;
}
export function getTelemetryHost() {
	return sender.host;
}

export function onTelemetryEnabledStateChange(
	callback: (enabled: boolean) => unknown,
) {
	logger.onDidChangeEnableStates((e) => callback(e.isUsageEnabled));
	sender.onChangeEnableSate(callback);
}
