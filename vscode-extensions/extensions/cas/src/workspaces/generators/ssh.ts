import { DepSchema, DepsSchema } from "@cas/deps";
import {
	batchProcess,
	decodeText,
	encodeText,
	resolveHome,
	testAccess,
	withTrailingSlash,
} from "@cas/helpers";
import { readJSON } from "@cas/helpers/vscode/fs.js";
import { Manifest } from "@cas/manifest";
import { BASFile } from "@cas/types/bas.js";
import { Paged } from "@cas/types/cas_server.js";
import { mkdir, readdir, rmdir, symlink } from "fs/promises";
import { NodeSSH } from "node-ssh";
import { homedir } from "os";
import { dirname, join, normalize, parse, relative } from "path";
import SSHConfig, { parse as sshParse } from "ssh-config";
import { promisify } from "util";
import { FileSystemError, Uri, workspace } from "vscode";
import { debug, error, info } from "../../logger";
import { DepsGenerator, DepsGeneratorOptions } from "./generator";

import "core-js/actual/array/from-async";

const { fs } = workspace;

function toDepsPathSet(
	deps: DepsSchema,
	sourceRoot: string,
	workspaceDir: string,
): Set<string> {
	return new Set(
		deps.entries
			.filter((entry: DepSchema) => entry.original_path.startsWith(sourceRoot))
			.map((dep) =>
				dep.original_path.startsWith(sourceRoot)
					? join(workspaceDir, relative(sourceRoot, dep.original_path))
					: dep.original_path,
			),
	);
}

function deArray<T>(arg: T | T[]): T {
	return Array.isArray(arg) ? arg.at(0)! : arg;
}

async function configureHost({
	hostname,
	port,
	username,
	keyFile,
}: Exclude<Manifest["remote"], undefined>) {
	const sshConfigFile = decodeText(
		await fs.readFile(
			Uri.file(
				workspace.getConfiguration().get("remote.SSH.configFile") ||
					`${homedir()}/.ssh/config`,
			),
		),
	);
	const sshConfig = sshParse(sshConfigFile);
	const currentHostSettings = sshConfig.compute({
		Host: hostname,
		User: username,
	});
	return {
		host: deArray(currentHostSettings.HostName ?? hostname),
		port:
			(port ?? currentHostSettings?.Port)
				? parseInt(deArray(port ?? currentHostSettings?.Port).toString())
				: undefined,
		username: deArray(currentHostSettings?.User ?? username),
		privateKeyPath:
			(keyFile ?? currentHostSettings.IdentityFile)
				? resolveHome(deArray(keyFile ?? currentHostSettings.IdentityFile))
				: undefined,
	};
}

const sshGenerator: DepsGenerator = {
	createSteps: 2,
	async createDeps({
		out,
		reportProgress,
		deps,
		sourceRoot,
		manifest,
		binaries,
	}) {
		if (!manifest || manifest.sourceRepo.type !== "sftp") {
			throw new Error("Invalid source repo type");
		}
		const hostConfig = await configureHost(manifest.sourceRepo);
		reportProgress?.({ message: "Connecting to target" });

		const ssh = new NodeSSH();

		await ssh.connect(hostConfig);
		let fileCount = 0;
		let skipCount = 0;
		let failCount = 0;
		const depsRoots = new Set<string>();

		const sftp = await ssh.requestSFTP();
		reportProgress?.({ message: "Downloading files" });
		deps.entries = (
			await Array.fromAsync(
				batchProcess(
					deps.entries.filter((entry: DepSchema) =>
						entry.original_path.startsWith(sourceRoot),
					),
					async (entry: DepSchema) => {
						entry.binary = binaries?.includes(entry.original_path) ?? false;
						entry.original_path = normalize(entry.original_path);
						entry.workspace_path = entry.original_path.startsWith(sourceRoot)
							? join(out, relative(sourceRoot, entry.original_path))
							: entry.original_path; //out of source directory - original path
						await mkdir(dirname(entry.workspace_path), { recursive: true });
						// don't download unnecessarily
						if (!(await testAccess(entry.workspace_path))) {
							await ssh
								.getFile(entry.workspace_path, entry.original_path, sftp, {})
								.then((_) => {
									fileCount++;
									reportProgress?.({ increment: 100 / deps.num_entries });
								})
								.catch((err: FileSystemError) => {
									if (err.code !== "EEXIST") {
										failCount++;
										error(
											`[generator.ssh] (${entry.original_path}) ${err.message}`,
											true,
										);
										throw err;
									} else {
										skipCount++;
									}
								});
						} else {
							skipCount++;
						}
						reportProgress?.({ increment: 100 / deps.num_entries });

						depsRoots.add(
							join(
								out,
								entry.filename.split(/\/|\\/).filter(Boolean).at(0) ?? "",
							),
						);
						return entry;
					},
					// TODO: make this configurable
					256,
					true,
				),
			)
		).flat();
		sftp.end();
		ssh.dispose();
		info(
			`[generator.ssh] Downloaded new=${fileCount} existing=${skipCount} failed=${failCount}`,
		);
		deps.count = deps.entries.length;
		deps.num_entries = deps.entries.length;
		deps.version = 2;
		return {
			deps,
			depsFolders: [...depsRoots],
		};
	},
	updateSteps: 1,
	async update({ out, deps, sourceRoot }) {
		const oldDeps = await readJSON<DepsSchema>(join(out, "deps.json"));
		const newSet = toDepsPathSet(deps, sourceRoot, out);
		const removedEntries = new Set(
			oldDeps.entries.map((dep) => dep.workspace_path),
		).difference(newSet);

		await Promise.all(
			[...removedEntries.values()].map((path) => {
				if (path?.startsWith(out)) {
					return fs.delete(Uri.file(path)).then(
						async () => {
							debug(`[generator.ssh] REMOVED ${path}`);
							let dirPath = dirname(path);
							while (
								(await testAccess(dirPath)) &&
								(await readdir(dirPath)).length === 0
							) {
								debug(`[generator.ssh] REMOVED empty dir ${dirPath}`);
								let ndirPath = dirname(dirPath);
								await rmdir(dirPath);
								dirPath = ndirPath;
							}
						},
						(_err) => {},
					);
				}
			}),
		);
	},
};

export default sshGenerator;
