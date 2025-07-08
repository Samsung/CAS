//@ts-ignore
import contentTypes from "inline:../media/contentTypes.xml";
import { FS, fs, ZipDirectoryEntry, ZipFileEntry } from "@zip.js/zip.js";
import { open, stat } from "fs/promises";
import { join } from "path/posix";
import { ReadableStream } from "stream/web";
import {
	commands,
	ExecServer,
	ExtensionContext,
	FileType,
	ProgressLocation,
	Uri,
	window,
	workspace,
} from "vscode";
import { Logger } from "winston";
import { getLogger } from "./logger";

const { readDirectory, readFile } = workspace.fs;

export class ExtensionInstaller {
	ctx: ExtensionContext;
	extPath: Uri;
	version: string;
	execServer: ExecServer;
	logger: Logger;
	constructor(
		ctx: ExtensionContext,
		extPath: Uri,
		version: string,
		execServer: ExecServer,
	) {
		this.ctx = ctx;
		this.extPath = extPath;
		this.version = version;
		this.execServer = execServer;
		this.logger = getLogger("ExtensionInstaller");
	}
	async installExtension(): Promise<boolean> {
		return window.withProgress(
			{
				location: ProgressLocation.Window,
				title: "Installing CAS extension $(cloud-download)",
			},
			async () => {
				this.logger.info("packaging");
				const zipFs = await this.packageExtension();

				this.logger.debug("Attempting to install from local directory");

				const data = await zipFs.exportUint8Array();
				let success = await this.localInstall(data);
				if (!success) {
					this.logger.info("Attemtping to install from remote instead");
					success = await this.remoteInstall(data);
				}
				this.logger.info(
					`installation status: ${success ? "success" : "failure"}`,
				);
				return success;
			},
		);
	}
	async packageExtension(): Promise<FS> {
		this.logger.debug(`packaging current installation from ${this.extPath}`);
		const zipFs = new fs.FS();

		await this.addDir(zipFs.addDirectory("extension"), this.extPath);
		await this.addMetadata(zipFs, this.extPath);
		this.logger.debug("Added content to ZipFS");
		return zipFs;
	}

	async addMetadata(zipFs: FS, extensionPath: Uri) {
		zipFs.addUint8Array(
			"extension.vsixmanifest",
			await readFile(Uri.joinPath(extensionPath, ".vsixmanifest")),
		);
		zipFs.addText("[Content_Types].xml", contentTypes as string);
	}

	async addDir(zipDir: ZipDirectoryEntry, path: Uri) {
		return Promise.all(
			(await readDirectory(path)).map(
				async ([name, type]): Promise<
					| ZipFileEntry<ReadableStream, void>
					| undefined
					| (ZipFileEntry<ReadableStream, void> | undefined)[]
				> => {
					const entryPath = Uri.joinPath(path, name);
					switch (type) {
						case FileType.File:
							this.logger.debug(
								`added file ${entryPath.fsPath}, size: ${(await stat(entryPath.fsPath)).size}`,
							);
							return zipDir.addUint8Array(name, await readFile(entryPath));
						case FileType.Directory:
							this.logger.debug(`added directory" ${entryPath.fsPath}`);
							return (
								await this.addDir(zipDir.addDirectory(name), entryPath)
							).flat();
						default:
							this.logger.error(
								`Unknown file type ${type} for ${entryPath.fsPath}`,
							);
							return undefined;
					}
				},
			),
		);
	}

	async localInstall(data: Uint8Array) {
		const vsixPath = Uri.joinPath(
			this.ctx.storageUri ?? this.ctx.globalStorageUri,
			`samsung.cas-${this.version}`,
		);
		await workspace.fs.writeFile(vsixPath, data);
		this.logger.debug(
			`attempting to install from local folder ${vsixPath.fsPath}`,
		);
		try {
			await commands.executeCommand(
				"workbench.extensions.installExtension",
				vsixPath,
			);
			return true;
		} catch (e) {
			this.logger.warn("Failed installing from local folder");
			this.logger.debug(`error: ${e}`);
			return false;
		}
	}

	async remoteInstall(data: Uint8Array) {
		const targetDir = join(
			(await this.execServer.env()).env.VSCODE_AGENT_FOLDER ?? "/tmp/vscode",
			"data/CachedExtensionVSIXs",
		);
		const vsixPath = join(targetDir, `samsung.cas-${this.version}`);
		await this.execServer.fs.mkdirp(targetDir);

		this.logger.debug(`writing VSIX to remote: ${vsixPath}`);

		const { stream, done } = await this.execServer.fs.write(vsixPath);

		stream.write(data);
		stream.end();

		await done;

		this.logger.debug(`installing extension via VSCode API`);
		try {
			await commands.executeCommand(
				"workbench.extensions.installExtension",
				Uri.file(vsixPath).with({ scheme: "vscode-remote" }),
			);
			return true;
		} catch (e) {
			this.logger.warn("Failed installing from remote");
			this.logger.debug(`error: ${e}`);
			return false;
		}
	}
}
