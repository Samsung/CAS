import { sleep } from "@cas/helpers/promise.js";
import type { CompCommandEntry } from "@cas/types/bas.js";
import PromisePool from "@supercharge/promise-pool";
import {
	SpawnSyncOptionsWithStringEncoding,
	spawn,
	spawnSync,
} from "child_process";
import { createReadStream, existsSync, mkdirSync } from "fs";
import { join, normalize } from "path";
import { split } from "shlex";
import { parser } from "stream-json";
import { streamArray } from "stream-json/streamers/StreamArray";
import { streamObject } from "stream-json/streamers/StreamObject";
import {
	CancellationToken,
	createConnection,
	Diagnostic,
	DiagnosticSeverity,
	DidChangeConfigurationNotification,
	DocumentDiagnosticParams,
	DocumentDiagnosticReportKind,
	InitializeParams,
	InitializeResult,
	Position,
	ProposedFeatures,
	RelatedFullDocumentDiagnosticReport,
	TextDocumentSyncKind,
	TextDocuments,
	WorkDoneProgress,
	WorkDoneProgressCreateRequest,
	WorkspaceFolder,
} from "vscode-languageserver/node";
import { TextDocument } from "vscode-languageserver-textdocument";

//region interfaces

interface ParsedDiag {
	direct: Diagnostic[];
	related: Map<string, Diagnostic[]>;
}

interface CompsMap {
	[path: string]: CompCommandEntry;
}
interface RevCompsMap {
	[path: string]: string[];
}

interface GCCLocation {
	caret: {
		file: string;
		line: number;
		"display-column": number;
		"byte-column": number;
		column: number;
	};
	finish: {
		file: string;
		line: number;
		"display-column": number;
		"byte-column": number;
		column: number;
	};
}

interface GCCDiag {
	kind: string;
	message: string;
	children: GCCDiag[];
	"column-origin": number;
	locations: GCCLocation[];
	"escape-source": boolean;
}

interface ClangLocation {
	physicalLocation: {
		artifactLocation: {
			index: number;
			uri: string;
		};
		region: {
			endColumn: number;
			startColumn: number;
			startLine: number;
		};
	};
}
interface ClangResult {
	level: string;
	locations: ClangLocation[];
	message: {
		text: string;
	};
	ruleId: string;
	ruleIndex: number;
}

interface CmdOutput {
	stdout: string;
	stderr: string;
	code: number | null;
}

//region const
const serverName = "CAS Build Verifier";
const connection = createConnection(ProposedFeatures.all);

const documents = new TextDocuments(TextDocument);

let hasConfigurationCapability = false;
let hasWorkspaceFolderCapability = false;
let workspaceFolders: WorkspaceFolder[] | null | undefined = undefined;
let wsDir: string = "";
let baseDir: string = "";
let concurency: number = 1;

let ccLoading: boolean = false;
let ccDb: CompsMap | undefined = undefined;
let ccFile: string | undefined = undefined;
let rdcLoading: boolean = false;
let rdcDb: any = undefined;
let rdcFile: string | undefined = undefined;
let clangVersionMap: Map<string, number> = new Map<string, number>();

const clangSeverityMap: any = {
	error: DiagnosticSeverity.Error,
	"fatal error": DiagnosticSeverity.Error,
	warning: DiagnosticSeverity.Warning,
	info: DiagnosticSeverity.Information,
	note: DiagnosticSeverity.Information,
	hint: DiagnosticSeverity.Hint,
};

function dbLoaded() {
	return !ccLoading && ccDb !== undefined && !rdcLoading && rdcDb !== undefined;
}

//region onInitialize
connection.onInitialize(async (params: InitializeParams) => {
	const capabilities = params.capabilities;

	baseDir = params.initializationOptions.baseDir;
	concurency = params.initializationOptions.concurency;

	hasConfigurationCapability = !!(
		capabilities.workspace && !!capabilities.workspace.configuration
	);

	hasWorkspaceFolderCapability = !!(
		capabilities.workspace && !!capabilities.workspace.workspaceFolders
	);

	workspaceFolders = params.workspaceFolders;
	connection.console.log(
		"Workspace Folders : " + JSON.stringify(workspaceFolders),
	);

	if (workspaceFolders?.length === 1) {
		wsDir = uriToPath(workspaceFolders[0].uri) + "/";
	}

	const result: InitializeResult = {
		capabilities: {
			textDocumentSync: {
				change: TextDocumentSyncKind.Full,
				openClose: true,
				save: { includeText: false },
			},
			diagnosticProvider: {
				interFileDependencies: false,
				workspaceDiagnostics: false,
				identifier: serverName,
				workDoneProgress: true,
			},
		},
	};
	if (hasWorkspaceFolderCapability) {
		result.capabilities.workspace = {
			workspaceFolders: {
				supported: true,
			},
		};
	}
	return result;
});

connection.onInitialized(async () => {
	if (hasConfigurationCapability) {
		connection.client.register(
			DidChangeConfigurationNotification.type,
			undefined,
		);
	}
	if (hasWorkspaceFolderCapability) {
		connection.workspace.onDidChangeWorkspaceFolders((_event) => {
			connection.console.log("Workspace folder change event received.");
		});
	}

	ccFile = getCompCommandsFilename();
	if (ccFile) {
		await readCompCommands(ccFile);
		connection.console.log(`Read compile commands from '${ccFile}'`);
	}
	rdcFile = getRevCompsFilename();
	if (rdcFile) {
		await readRevComps(rdcFile);
		connection.console.log(`Read revcomps from '${rdcFile}'`);
	}
});

connection.onDidChangeConfiguration(() => {
	connection.workspace.getConfiguration().then((config) => {
		if (concurency !== config?.cas?.concurency) {
			concurency = config?.cas?.concurency;
			console.log(`Updated concurency to ${concurency}`);
		}
	});
});

function uriToPath(uri: string) {
	return uri.replace("file://", "");
}

function normalizeUri(uri: string) {
	if (uri.startsWith("file://")) {
		uri = uri.replace("file://", "");
	}
	if (!uri.startsWith(wsDir)) {
		uri = uri.replace(baseDir, wsDir);
	}
	if (!uri.startsWith(wsDir)) {
		uri = join(wsDir, uri);
	}
	return `file://${normalize(uri)}`;
}

function pathToUri(path: string): string {
	return `file://${path}`;
}
let clangPattern = new RegExp(
	/^(.*\/)?clang\+?\+?((?:-\d+)|(?:-\d+\.\d+)?)(?:\.real)?$/,
);
let gccPattern = new RegExp(
	/^(.*\/)?gcc((?:-\d+)|(?:-\d+\.\d+)?)(?:\.real)?$|^(.*\/)?g\+\+((?:-\d+)|(?:-\d+\.\d+)?)(?:\.real)?$/,
);

function isClangBin(bin: string) {
	return clangPattern.test(bin);
}

function isGCCBin(bin: string) {
	return gccPattern.test(bin);
}

function prepareArgsGCC(args: string[]): string[] {
	let ret = args.map((x) => x);
	ret.push("-fsyntax-only");
	ret.push("-fdiagnostics-format=json");
	return ret;
}

function prepareArgsClang(bin: string, args: string[]): string[] {
	let ret = args.map((x) => x);
	let ver: number = probeVersion(bin);
	ret.push("-fsyntax-only");
	ret.push("-fdiagnostics-print-source-range-info");
	ret.push("-fno-color-diagnostics");
	ret.push("-fno-caret-diagnostics");
	if (ver >= 16) {
		//clang 16 introduce json output format
		ret.push("-fdiagnostics-format=sarif");
		ret.push("-Wno-sarif-format-unstable");
	}
	return ret;
}

function probeVersion(bin: string): number {
	if (!clangVersionMap.has(bin)) {
		let proc = spawnSync(`${bin} -dumpversion`, {
			shell: true,
			encoding: "utf-8",
		});
		let verStr = proc.stdout.trim();
		if (verStr.indexOf(".") !== -1) {
			try {
				clangVersionMap.set(
					bin,
					Number.parseInt(proc.stdout.trim().split(".")[0]),
				);
			} catch {
				console.log(
					`Can't probe clang version from '${bin} -dumpversion' -> output: '${verStr}'`,
				);
			}
		} else {
			console.log(
				`Can't probe clang version from '${bin} -dumpversion' -> output: '${verStr}'`,
			);
		}
	}
	let ver = clangVersionMap.get(bin);
	return ver ? ver : 0;
}

//region runCompiler
async function runCompiler(
	bin: string,
	args: string[],
	directory: string,
): Promise<CmdOutput> {
	return new Promise((resolve) => {
		const retCmdOutput: CmdOutput = { stdout: "", stderr: "", code: null };

		if (!existsSync(directory)) {
			mkdirSync(directory, { recursive: true });
		}

		let opts: SpawnSyncOptionsWithStringEncoding = {
			maxBuffer: 1024 * 5000,
			timeout: 30000,
			cwd: directory,
			env: {
				PATH: process.env.PATH + ":" + directory,
				LANG: "en_US.UTF-8",
				//TODO - add compilation envs if cas tracer envs support will be available
			},
			encoding: "utf-8",
		};

		let proc = spawn(bin, args, opts);

		proc.stdout?.on("data", (data) => {
			retCmdOutput.stdout += data.toString();
		});

		proc.stderr?.on("data", (data) => {
			retCmdOutput.stderr += data.toString();
		});

		proc.on("error", (err) => {
			retCmdOutput.code = 1;
			retCmdOutput.stderr = err.message;
			resolve(retCmdOutput);
		});

		proc.on("exit", (code) => {
			retCmdOutput.code = code;
			resolve(retCmdOutput);
		});
	});
}

// region getDiag
function getClangDiagnostics(m: RegExpExecArray): Diagnostic {
	if (m.length === 8) {
		return {
			severity: clangSeverityMap[m[6]],
			range: {
				start: { line: Number(m[2]) - 1, character: Number(m[3]) },
				end: { line: Number(m[4]) - 1, character: Number(m[5]) },
			},
			message: m[7],
			source: serverName,
		} satisfies Diagnostic;
	}
	if (m.length === 6) {
		return {
			severity: clangSeverityMap[m[4]],
			range: {
				start: { line: Number(m[2]) - 1, character: Number(m[3]) },
				end: { line: Number(m[2]) - 1, character: Number(m[3]) },
			},
			message: m[5],
			source: serverName,
		} satisfies Diagnostic;
	} else {
		return {
			severity: DiagnosticSeverity.Information,
			range: {
				start: { line: Number(m[2]) - 1, character: 0 },
				end: { line: Number(m[2]) - 1, character: 999 },
			},
			message: "Problem with include",
			source: serverName,
		} satisfies Diagnostic;
	}
}

// region diagnostics.on
connection.languages.diagnostics.on(
	async (
		params: DocumentDiagnosticParams,
		token: CancellationToken,
	): Promise<RelatedFullDocumentDiagnosticReport> => {
		let maxWait = 20;
		while (!dbLoaded() && maxWait > 0) {
			maxWait--;
			await sleep(1000);
		}

		connection.console.log(`>> diagnostics.on  >>  ${params.textDocument.uri}`);
		if (!ccDb) {
			return { kind: DocumentDiagnosticReportKind.Full, items: [] };
		}
		const document: TextDocument | undefined = documents.get(
			params.textDocument.uri,
		);

		if (document) {
			let u = normalizeUri(document.uri);
			validate(u, token);
		}
		return { kind: DocumentDiagnosticReportKind.Full, items: [] };
	},
);

//region validate
async function validate(
	uri: string,
	cancelationToken: CancellationToken,
	hasProgress: boolean = false,
): Promise<void> {
	return new Promise((resolve) => {
		if (cancelationToken.isCancellationRequested) {
			connection.console.log(`CANCELED ${uri}`);
			resolve();
		}
		let fname = uriToPath(uri);
		let diagMap: Map<string, Diagnostic[]> = new Map();
		let keys: Set<string> = new Set();

		if (ccDb && fname in ccDb) {
			validateSingle(fname, uri, cancelationToken).then((compRet) => {
				if (compRet) {
					if (!diagMap.has(uri)) {
						diagMap.set(uri, []);
					}

					compRet.direct.forEach((d) => {
						diagMap.get(uri)?.push(d);
					});

					compRet.related.forEach((val, key) => {
						val.forEach((d) => {
							let k = generateKey(d, key);
							if (!keys.has(k)) {
								keys.add(k);
								if (!diagMap.has(key)) {
									diagMap.set(key, []);
								}
								diagMap.get(key)?.push(d);
							}
						});
					});
				}
				sendDiags(uri, diagMap);
				if (hasProgress) {
					connection.sendRequest("client/progress");
				}
				resolve();
			});
		} else {
			let shortName = fname.replace(wsDir, "");
			if (rdcDb) {
				let related = rdcDb[shortName];
				if (related && related?.length > 0) {
					let rvcompFullPath = join(wsDir, related[0]);
					connection.console.log(
						`   >> validate related >>  ${rvcompFullPath}`,
					);
					validateSingle(
						rvcompFullPath,
						normalizeUri(rvcompFullPath),
						cancelationToken,
					).then((relatedRet) => {
						if (relatedRet) {
							relatedRet.related.forEach((val, key) => {
								if (!diagMap.has(key)) {
									diagMap.set(key, []);
								}
								val.forEach((d) => {
									let k = generateKey(d, key);
									if (!keys.has(k)) {
										keys.add(k);
										diagMap.get(key)?.push(d);
									}
								});
							});
						}
						sendDiags(uri, diagMap);
						if (hasProgress) {
							connection.sendRequest("client/progress");
						}
						resolve();
					});
				}
			}
		}
	});
}

function sendDiags(uri: string, diagMap: Map<string, Diagnostic[]>) {
	// console.log("sendDiags")
	connection.sendDiagnostics({ uri: uri, diagnostics: [] });
	diagMap.forEach(async (val, key) => {
		connection.sendDiagnostics({ uri: key, diagnostics: val });
	});
}

function generateKey(d: Diagnostic, file: string): string {
	return `${d.range.start.line},${d.range.start.character}-${d.range.end.line},${d.range.end.character}:${d.message}:${file}`;
}

//region validateSingle
async function validateSingle(
	path: string,
	normalizedUri: string,
	_cancelationToken: CancellationToken,
): Promise<{ direct: Diagnostic[]; related: Map<string, Diagnostic[]> }> {
	let diags: Diagnostic[] = [];
	let relatedDiags: Map<string, Diagnostic[]> = new Map();
	if (ccDb && path in ccDb) {
		let commandInfo: CompCommandEntry = ccDb[path];
		const shellArgs =
			"arguments" in commandInfo
				? commandInfo.arguments
				: split(commandInfo.command);
		let bin = shellArgs[0];
		let args = shellArgs.slice(1);
		let runArgs: string[] = [];

		if (isClangBin(bin)) {
			runArgs = prepareArgsClang(bin, args);
		} else if (isGCCBin(bin)) {
			runArgs = prepareArgsGCC(args);
		} else {
			runArgs = args;
		}
		let cmdOut = await runCompiler(bin, runArgs, commandInfo.directory);

		let arr = undefined;
		if (cmdOut.stdout.length > 0 || cmdOut.stderr.length > 0) {
			if (isGCCBin(bin)) {
				arr = parseGCCOutput(cmdOut.stderr, normalizedUri);
			}
			if (isClangBin(bin)) {
				let o = cmdOut.stdout + "\n" + cmdOut.stderr;
				if (probeVersion(bin) >= 16) {
					arr = parseJSONClangOutput(o, normalizedUri);
				} else {
					arr = parsePlainClangOutput(o, normalizedUri);
				}
			}
			if (arr) {
				diags = arr.direct;
				arr.related.forEach((v, k) => {
					relatedDiags.set(k, v);
				});
			}
		}
		if (diags.length === 0 && relatedDiags.size === 0 && cmdOut.code !== 0) {
			const d: Diagnostic = {
				severity: DiagnosticSeverity.Error,
				range: {
					start: Position.create(0, 0),
					end: Position.create(0, 0),
				},
				message: cmdOut.stdout + "\n" + cmdOut.stderr,
				source: serverName,
			};
			diags.push(d);
		}
		// console.log(`\n--------------------------------------------------\nValidation completed for ${path} \n${commandInfo.directory}\n${bin} ${args.join(" ")} \nret code=${cmdOut.code}\nstdout='${cmdOut.stdout}'\nstderr='${cmdOut.stderr}' \n ${JSON.stringify({direct: diags, related: relatedDiags })}\n${JSON.stringify(arr)}\n--------------------------------------------------`);
	}
	return { direct: diags, related: relatedDiags };
}

//region parseGCCOutput
function parseGCCOutput(stderr: string, docFile: string): ParsedDiag {
	const directErrors: Diagnostic[] = [];
	const relatedDiags = new Map<string, Diagnostic[]>();
	let gccDiag = undefined;
	let compFilePath = uriToPath(docFile);

	try {
		gccDiag = JSON.parse(stderr);
	} catch {
		connection.console.log(`Cannot parse gcc output!`);
	}
	gccDiag.forEach((element: GCCDiag) => {
		element.locations.forEach((loc) => {
			let d = {
				severity: clangSeverityMap[element.kind],
				range: {
					start: {
						line: loc.caret.line - 1,
						character: loc.caret.column - 1,
					},
					end: {
						line: loc.finish ? loc.finish.line - 1 : loc.caret.line - 1,
						character: loc.finish ? loc.finish.column : loc.caret.column - 1,
					},
				},
				message: element.message,
				source: serverName,
			} satisfies Diagnostic;
			if (normalize(loc.caret.file) === normalize(compFilePath)) {
				directErrors.push(d);
			} else {
				let relUri = pathToUri(loc.caret.file);
				if (!relatedDiags.has(relUri)) {
					relatedDiags.set(relUri, []);
				}
				relatedDiags.get(relUri)?.push(d);
			}
		});

		element.children.forEach((child: GCCDiag) => {
			child.locations.forEach((loc) => {
				let d = {
					severity: clangSeverityMap[child.kind],
					range: {
						start: {
							line: loc.caret.line - 1,
							character: loc.caret.column - 1,
						},
						end: {
							line: loc.finish ? loc.finish.line - 1 : loc.caret.line - 1,
							character: loc.finish ? loc.finish.column : loc.caret.column - 1,
						},
					},
					message: child.message,
					source: serverName,
				} satisfies Diagnostic;
				if (normalize(loc.caret.file) === normalize(compFilePath)) {
					directErrors.push(d);
				} else {
					let relUri = pathToUri(loc.caret.file);
					if (!relatedDiags.has(relUri)) {
						relatedDiags.set(relUri, []);
					}
					relatedDiags.get(relUri)?.push(d);
				}
			});
		});
	});

	return { direct: directErrors, related: relatedDiags };
}

const clangLineRangePattern = new RegExp(
	/(.*):\d+:\d+:\{(\d+):(\d+)-(\d+):(\d+)\}: (\w+): (.*)/,
);
const clangLinePattern = new RegExp(/(.*):(\d+):(\d+): (\w+): (.*)/);

//region parsePlainClangOutput
function parsePlainClangOutput(output: string, docFile: string): ParsedDiag {
	const directErrors: Diagnostic[] = [];
	const relatedDiags = new Map<string, Diagnostic[]>();
	let dkeys: Set<string> = new Set();
	let rkeys: Set<string> = new Set();
	const o = output.split("\n").filter((l) => l.startsWith("/"));

	o.forEach((line) => {
		let m = clangLinePattern.exec(line);
		if (!m) {
			m = clangLineRangePattern.exec(line);
		}
		if (m) {
			const msgFname = normalizeUri(m[1]);
			let d: Diagnostic = getClangDiagnostics(m);
			if (d) {
				let k = generateKey(d, msgFname);
				if (msgFname !== docFile) {
					if (!rkeys.has(k)) {
						rkeys.add(k);
						if (!relatedDiags.has(msgFname)) {
							relatedDiags.set(msgFname, []);
						}
						relatedDiags.get(msgFname)?.push(d);
					}
				} else {
					if (!dkeys.has(k)) {
						dkeys.add(k);
						directErrors.push(d);
					}
				}
			}
		}
	});
	return { direct: directErrors, related: relatedDiags };
}

interface ClangRun {
	artifacts: [];
	columnKind: string;
	results: ClangResult[];
	tool: {
		driver: {
			fullName: string;
			informationUri: string;
			language: string;
			name: string;
			rules: [];
			version: string;
		};
	};
}
interface ClangSarif {
	$schema: string;
	runs: ClangRun[];
	version: string;
}

//region parseClangOutput
function parseJSONClangOutput(output: string, docFile: string): ParsedDiag {
	const directErrors: Diagnostic[] = [];
	const relatedDiags = new Map<string, Diagnostic[]>();
	let dkeys: Set<string> = new Set();
	let rkeys: Set<string> = new Set();
	let jsn: ClangSarif | undefined = undefined;
	try {
		jsn = JSON.parse(output);
	} catch {
		return { direct: directErrors, related: relatedDiags };
	}
	if (jsn) {
		jsn.runs.forEach((r: ClangRun) => {
			r.results.forEach((element: ClangResult) => {
				element.locations.forEach((loc: ClangLocation) => {
					const msgFname = normalizeUri(
						loc.physicalLocation.artifactLocation.uri,
					);
					let d = {
						severity: clangSeverityMap[element.level],
						range: {
							start: {
								line: loc.physicalLocation.region.startLine - 1,
								character: loc.physicalLocation.region.startColumn - 1,
							},
							end: {
								line: loc.physicalLocation.region.startLine - 1,
								character: loc.physicalLocation.region.endColumn,
							},
						},
						message: element.message.text,
						source: serverName,
					} satisfies Diagnostic;
					let k = generateKey(d, msgFname);
					if (msgFname !== docFile) {
						if (!rkeys.has(k)) {
							rkeys.add(k);
							if (!relatedDiags.has(msgFname)) {
								relatedDiags.set(msgFname, []);
							}
							relatedDiags.get(msgFname)?.push(d);
						}
					} else {
						if (!dkeys.has(k)) {
							dkeys.add(k);
							directErrors.push(d);
						}
					}
				});
			});
		});
	}
	return { direct: directErrors, related: relatedDiags };
}

//region onDidChangeWatchedFiles
connection.onDidChangeWatchedFiles((_change) => {
	connection.console.log("onDidChangeWatchedFiles START");
	_change.changes.forEach(async (fname) => {
		let filename = uriToPath(fname.uri);
		if (filename === ccFile && existsSync(ccFile)) {
			await readCompCommands(ccFile);
		}
		if (filename === rdcFile && existsSync(rdcFile)) {
			await readRevComps(rdcFile);
		}
	});
	connection.languages.diagnostics.refresh();
	connection.console.log("onDidChangeWatchedFiles END");
});

//region getCompCommandsFilename
function getCompCommandsFilename(): string | undefined {
	if (workspaceFolders && workspaceFolders.length === 1) {
		const expectedCCFile = join(
			uriToPath(workspaceFolders[0].uri),
			"compile_commands.json",
		);
		if (existsSync(expectedCCFile)) {
			connection.console.log(`Found compile commands in '${expectedCCFile}'`);
			return expectedCCFile;
		} else {
			connection.console.log(
				`Compile commands not found in '${expectedCCFile}'`,
			);
		}
	}
}

//region getCompCommandsFilename
function getRevCompsFilename(): string | undefined {
	if (workspaceFolders && workspaceFolders.length === 1) {
		const expectedRDCFile = join(
			uriToPath(workspaceFolders[0].uri),
			"rcm.json",
		);
		if (existsSync(expectedRDCFile)) {
			connection.console.log(
				`Found reverse compilations in '${expectedRDCFile}'`,
			);
			return expectedRDCFile;
		} else {
			connection.console.log(
				`Reverse compilations not found in '${expectedRDCFile}'`,
			);
		}
	}
}

//region readCompCommands
function readCompCommands(path: string): Promise<void> {
	return new Promise<void>(async (resolve, reject) => {
		const fileStream = createReadStream(path, "utf-8");
		let ret: CompsMap = {} satisfies CompsMap;

		const token = "readCompCommands";

		await connection.sendRequest(WorkDoneProgressCreateRequest.type, {
			token,
		});

		connection.sendProgress(WorkDoneProgress.type, token, {
			kind: "begin",
			title: "Loading compiled commands...",
		});

		ccLoading = true;
		fileStream
			.pipe(parser())
			.pipe(streamArray())
			.on("data", (data) => {
				ret[data.value.file] = data.value;
			})
			.on("error", (error) => {
				ccLoading = false;
				connection.sendProgress(WorkDoneProgress.type, token, {
					kind: "end",
					message: `FAILED! ${error}`,
				});
				reject();
			})
			.on("end", () => {
				ccDb = ret;
				ccLoading = false;
				connection.sendProgress(WorkDoneProgress.type, token, {
					kind: "end",
					message: "Completed!",
				});
				resolve();
			});
	});
}

//region readRevComps
function readRevComps(path: string): Promise<void> {
	return new Promise<void>(async (resolve, reject) => {
		if (workspaceFolders && workspaceFolders.length === 1) {
			rdcLoading = true;
			const fileStream = createReadStream(path, "utf-8");
			const token = "readRevComps";
			await connection.sendRequest(WorkDoneProgressCreateRequest.type, {
				token,
			});
			connection.sendProgress(WorkDoneProgress.type, token, {
				kind: "begin",
				title: "Loading reverse compilations...",
			});
			let ret: RevCompsMap = {};

			fileStream
				.pipe(parser())
				.pipe(streamObject())
				.on("data", (data) => {
					ret[data.key] = [data.value[0]];
				})
				.on("error", (error) => {
					console.log(error);
					rdcLoading = false;
					connection.sendProgress(WorkDoneProgress.type, token, {
						kind: "end",
						message: "FAILED!",
					});
					reject();
				})
				.on("end", () => {
					rdcDb = ret;
					rdcLoading = false;
					connection.sendProgress(WorkDoneProgress.type, token, {
						kind: "end",
						message: "Completed!",
					});
					resolve();
				});
		}
	});
}

//region verifySingle
connection.onRequest(
	"server/verifySingle",
	async (params, token: CancellationToken) => {
		const startTime = performance.now();
		if (ccDb) {
			connection.console.log(`About to verify '${params.file_name}'`);
			await validate(params.file_name, token);
			connection.console.log(`Verify '${params.file_name}' DONE`);
		}
		const endTime = performance.now();
		return endTime - startTime;
	},
);

//region verifyAll
connection.onRequest("server/verifyAll", async (token: CancellationToken) => {
	const startTime = performance.now();
	if (ccDb) {
		connection.console.log(
			`About to verify All compile commands on ${concurency} cpu threads`,
		);
		await PromisePool.withConcurrency(concurency)
			.for(Object.keys(ccDb))
			.process(async (path, _index, pool) => {
				if (token.isCancellationRequested) {
					pool.stop();
				}
				await validate(pathToUri(path), token, true);
			});
	}
	const endTime = performance.now();
	return endTime - startTime;
});

documents.listen(connection);
connection.listen();
