import Plausible from "plausible-tracker";
import { type CasApiEvent, vscode } from "./vscode";

type TelemetryEvent = MessageEvent<{ enabled: boolean; apiHost: string }>;

let plausibleTrackEvent: ReturnType<typeof Plausible>["trackEvent"];
let plausibleTrackPageview: ReturnType<typeof Plausible>["trackPageview"];

let enabled = true;
let apiHost: string;
export async function telemetryInit() {
	delete localStorage.plausible_ignore;
	({
		data: { enabled, apiHost },
	} = (await vscode.execFunction("telemetry", {})) as CasApiEvent<{
		enabled: boolean;
		apiHost: string;
	}>);
	if (!enabled) {
		localStorage.plausible_ignore = true;
	}
	({ trackEvent: plausibleTrackEvent, trackPageview: plausibleTrackPageview } =
		Plausible({
			apiHost,
			domain: "samsung.cas",
		}));
	vscode.addEventListener("telemetry", (event) => {
		if ((event as TelemetryEvent).data.enabled) {
			enabled = true;
			delete localStorage.plausible_ignore;
		} else {
			enabled = false;
			localStorage.plausible_ignore = true;
		}
		const newApiHost = (event as TelemetryEvent).data.apiHost;
		if (newApiHost && newApiHost !== apiHost) {
			apiHost = newApiHost;
			({
				trackEvent: plausibleTrackEvent,
				trackPageview: plausibleTrackPageview,
			} = Plausible({
				apiHost,
				domain: "samsung.cas",
			}));
		}
	});
}

export const trackEvent: typeof plausibleTrackEvent = (
	eventName,
	options,
	eventData,
) => {
	if (enabled) {
		plausibleTrackEvent(
			eventName,
			{ ...options, props: { source: "webview", ...(options?.props ?? {}) } },
			{ ...eventData },
		);
	}
};

export const trackPageview: typeof plausibleTrackPageview = (
	eventData,
	options,
) => {
	if (enabled) {
		plausibleTrackPageview(eventData, {
			...options,
			props: { source: "webview", ...(options?.props ?? {}) },
		});
	}
};
