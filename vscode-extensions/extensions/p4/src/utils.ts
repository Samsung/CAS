import { decodeText, withAbort } from "@cas/helpers";
import {
	Err,
	EventualResult,
	None,
	Ok,
	type Option,
	type Result,
	Some,
} from "@opliko/eventual-result";
import { Ajv, type JTDParser, type JTDSchemaType } from "ajv/dist/jtd";
import {
	type ExtensionContext,
	type InputBoxOptions,
	type Uri,
	window,
	workspace,
} from "vscode";
import { getLogger } from "./logger";

const ajv = new Ajv();

const { readFile } = workspace.fs;

export const parseJSON = (text: string): Result<unknown, SyntaxError> => {
	try {
		return new Ok(JSON.parse(text));
	} catch (e) {
		return new Err(e as SyntaxError);
	}
};

export async function readJSON(path: Uri) {
	return new EventualResult(readFile(path)).map(decodeText).andThen(parseJSON);
}

type JSONparser<T = unknown> = (json: string) => Result<T, SyntaxError>;
const parserCache = {} as Record<string, Ok<JSONparser>>;

async function createParser<T = unknown>(name: string) {
	const logger = getLogger("parser");
	logger.debug(`loading schema ./schemas/${name}.schema.json`);
	const { default: schema } = (await import(`./schemas/${name}.schema.json`, {
		with: { type: "json" },
	})) as { default: JTDSchemaType<T> };
	logger.debug("loaded schema file:", schema);
	let parser: JTDParser<T>;
	try {
		parser = ajv.compileParser(schema);
	} catch (e) {
		return new Err(e);
	}
	logger.debug("successfully loaded schema");
	const parseJSON: Ok<JSONparser<T>> = new Ok((json: string) => {
		const result = parser(json);
		return result ? new Ok(result) : new Err(new SyntaxError(parser.message));
	});
	parserCache[name] = parseJSON;
	return parseJSON;
}

export async function readJSONWithSchema<T = unknown>(
	path: Uri,
	schema: string,
): Promise<Result<T, unknown>> {
	const parseJSON =
		(parserCache[schema] as Ok<JSONparser<T>>) ??
		(await createParser<T>(schema));

	return new EventualResult(readFile(path))
		.map(decodeText)
		.andThen((v) => parseJSON.andThen((parser) => parser(v)));
}

export async function confirm<T>(
	message: string,
	value: T,
	signal?: AbortSignal,
): Promise<Result<Option<T>, unknown>> {
	let promise = window.showInformationMessage(message, "Yes", "No");
	if (signal) {
		promise = Promise.race([
			promise,
			new Promise<"No">((resolve) =>
				signal.addEventListener("abort", () => resolve("No")),
			),
		]);
	}
	return new EventualResult(promise).map((v) =>
		v === "Yes" ? new Some(value) : None,
	);
}

export async function showInput(
	options: InputBoxOptions,
	signal?: AbortSignal,
) {
	let promise = window.showInputBox(options);
	if (signal) {
		promise = withAbort(promise as Promise<string | undefined>, signal);
	}
	return new EventualResult(promise).map((v) => (v ? new Some(v) : None));
}

export type MaybeAsync<T> = Promise<T> | T;
