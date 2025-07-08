import type { WebviewApi } from "vscode-webview";

export interface CasApiEvent<T = unknown> {
	func: string;
	id: string;
	data: T;
}

/**
 * A utility wrapper around the acquireVsCodeApi() function, which enables
 * message passing and state management between the webview and extension
 * contexts.
 *
 * This utility also enables webview code to be run in a web browser-based
 * dev server by using native web browser features that mock the functionality
 * enabled by acquireVsCodeApi.
 */
class VSCodeAPIWrapper<T = unknown> {
	private vsCodeApi: WebviewApi<T> | undefined;
	readonly #messagePromises: Map<string, (result: unknown) => void>;
	readonly #messageListeners: Map<
		string,
		((event: MessageEvent<any>) => unknown)[]
	>;
	constructor() {
		// Check if the acquireVsCodeApi function exists in the current development
		// context (i.e. VS Code development window or web browser)
		// @ts-ignore
		if (typeof globalThis.acquireVsCodeApi === "function") {
			this.vsCodeApi = acquireVsCodeApi<T>();
			if (this.vsCodeApi) {
				// @ts-ignore
				globalThis.acquireVsCodeApi = <T>() =>
					this.vsCodeApi! as unknown as WebviewApi<T>;
			}
			this.addListener();
		} else {
			const init = setInterval(() => {
				// @ts-ignore
				if (typeof globalThis.acquireVsCodeApi === "function") {
					this.vsCodeApi = acquireVsCodeApi();
					this.addListener();
					clearInterval(init);
				}
			}, 50);
		}
		this.#messagePromises = new Map();
		this.#messageListeners = new Map();
	}
	private addListener() {
		typeof window === "object" &&
			window.addEventListener("message", (event) => {
				if (this.#messageListeners.has(event.data?.func)) {
					for (const listener of this.#messageListeners.get(event.data?.func) ??
						[]) {
						listener(event);
					}
				}
				if (!("id" in event.data) || typeof event.data.id !== "string") {
					return;
				}
				const { id } = event.data;
				if (!this.#messagePromises.has(id)) {
					return;
				}
				this.#messagePromises.get(id)!(event.data);
				this.#messagePromises.delete(id);
			});
	}
	public addEventListener<T>(
		func: string,
		callback: (event: MessageEvent<T>) => unknown,
	) {
		if (!this.#messageListeners.has(func)) {
			this.#messageListeners.set(func, []);
		}
		this.#messageListeners.get(func)?.push(callback);
	}
	/**
	 * Post a message (i.e. send arbitrary data) to the owner of the webview.
	 *
	 * @remarks When running webview code inside a web browser, postMessage will instead
	 * log the given message to the console.
	 *
	 * @param message Abitrary data (must be JSON serializable) to send to the extension context.
	 */
	public postMessage(message: unknown) {
		if (this.vsCodeApi) {
			this.vsCodeApi.postMessage(message);
		} else {
			console.log(message);
		}
	}

	/**
	 * Get the persistent state stored for this webview.
	 *
	 * @remarks When running webview source code inside a web browser, getState will retrieve state
	 * from local storage (https://developer.mozilla.org/en-US/docs/Web/API/Window/localStorage).
	 *
	 * @return The current state or `undefined` if no state has been set.
	 */
	public getState(): unknown | undefined {
		if (this.vsCodeApi) {
			return this.vsCodeApi.getState();
		} else {
			const state = localStorage.getItem("vscodeState");
			return state ? JSON.parse(state) : undefined;
		}
	}

	/**
	 * Set the persistent state stored for this webview.
	 *
	 * @remarks When running webview source code inside a web browser, setState will set the given
	 * state using local storage (https://developer.mozilla.org/en-US/docs/Web/API/Window/localStorage).
	 *
	 * @param newState New persisted state. This must be a JSON serializable object. Can be retrieved
	 * using {@link getState}.
	 *
	 * @return The new state.
	 */
	public setState(newState: T): T {
		if (this.vsCodeApi) {
			return this.vsCodeApi.setState(newState);
		} else {
			localStorage.setItem("vscodeState", JSON.stringify(newState));
			return newState;
		}
	}

	public async execFunction(
		func: string,
		{ ...data }: Record<string, unknown>,
		signal?: AbortSignal,
	): Promise<unknown> {
		const id = crypto.randomUUID();
		const response = new Promise((resolve) =>
			this.#messagePromises.set(id, resolve),
		);
		if (signal) {
			signal.onabort = (_) => this.#messagePromises.delete(id);
		}
		this.postMessage({ func, id, ...data });
		return response;
	}

	public getStateKey(key: string): unknown | undefined {
		const state = this.getState() as Record<string, unknown> | undefined;
		if (state && key in (state as object)) {
			return state[key];
		}
		return undefined;
	}
	public setStateKey(key: string, newState: T): void {
		let state = this.getState() as Record<string, unknown> | undefined;
		if (state) {
			state[key] = newState;
		} else {
			state = { [key]: newState };
		}
		this.setState(state as T);
	}
	public setStateKeys(keys: Record<string, T>): void {
		let state = this.getState() as Record<string, unknown> | undefined;
		this.setState({ ...state, ...keys } as T);
	}
}

// Exports class singleton to prevent multiple invocations of acquireVsCodeApi.
export const vscode = new VSCodeAPIWrapper();

/*
This file was modified from https://github.com/microsoft/vscode-webview-ui-toolkit-samples/blob/main/frameworks/hello-world-svelte/webview-ui/src/utilities/vscode.ts
and is licensed under:
MIT License

Copyright (c) Microsoft Corporation.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE
*/
