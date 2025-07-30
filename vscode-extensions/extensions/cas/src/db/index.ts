import { normalize } from "node:path";
import { withTrailingSlash } from "@cas/helpers";
import { http } from "@cas/http";
import { ModelStatus, ServerStatus } from "@cas/types/cas_server.js";
import { getLogger } from "@logtape/logtape";
import * as vscode from "vscode";
import { Settings } from "../settings";
import { CASDatabase } from "./generic";
import { RemoteCASDatabase } from "./remote";
import { LocalServerCASDatabase } from "./server";

export interface FTDBInfo {
	path: string;
	scheme: "ftdb_file" | "ftdb_url";
}
export interface BASDBInfo {
	path: string;
	scheme: "bas_file" | "bas_url";
}

export interface EmptyDBInfo {
	path: undefined;
	scheme: undefined;
}

export type DBInfo = FTDBInfo | BASDBInfo;

type ItemMap = {
	[key: string]: {
		funct: (item: DBInfo, instance: DBProvider) => Promise<void>;
		dbinfo?: DBInfo;
	};
};

export class DBProvider implements vscode.Disposable {
	basStatusBarWidget: vscode.StatusBarItem;
	pickBASCommand = "cas.db.pick.bas";
	ftdbStatusBarWidget: vscode.StatusBarItem;
	pickFTDBCommand = "cas.db.pick.ft";
	// serverStatusBarWidget: vscode.StatusBarItem;
	serverId = "cas.serverOpts";
	currentDb: CASDatabase | undefined = undefined;
	s: Settings;
	ctx: vscode.ExtensionContext;

	#dbSubscriptionIndex?: number;
	private readonly logger = getLogger(["CAS", "db", "provider"]);

	constructor(context: vscode.ExtensionContext, settings: Settings) {
		this.s = settings;
		this.ctx = context;
		context.subscriptions.push(this);
		context.subscriptions.push(
			vscode.commands.registerCommand(this.pickBASCommand, async () =>
				this.showQuickPick(this.pickBASCommand),
			),
		);
		this.basStatusBarWidget = vscode.window.createStatusBarItem(
			vscode.StatusBarAlignment.Left,
			0,
		);
		this.basStatusBarWidget.name = "Selected BAS Database";
		this.basStatusBarWidget.command = this.pickBASCommand;
		context.subscriptions.push(this.basStatusBarWidget);

		context.subscriptions.push(
			vscode.commands.registerCommand(this.pickFTDBCommand, async () =>
				this.showQuickPick(this.pickFTDBCommand),
			),
			vscode.commands.registerCommand("cas.server.start", async () =>
				this.currentDb?.start(),
			),
		);
		this.ftdbStatusBarWidget = vscode.window.createStatusBarItem(
			vscode.StatusBarAlignment.Left,
			0,
		);
		this.ftdbStatusBarWidget.name = "Selected FTDB";
		this.ftdbStatusBarWidget.command = this.pickFTDBCommand;
		context.subscriptions.push(this.ftdbStatusBarWidget);

		this.setCurrentDb(this.s.basDB, this.s.FTDB).then(() =>
			this.showStatusBar(),
		);
	}
	async dispose() {
		return this.currentDb?.dispose();
	}

	private _onDBChange: vscode.EventEmitter<DBInfo | undefined | void> =
		new vscode.EventEmitter<DBInfo | undefined | void>();
	readonly onDBChange: vscode.Event<DBInfo | undefined | void> =
		this._onDBChange.event;

	showStatusBar() {
		this.basStatusBarWidget.text = `BAS: ${this.currentDb?.casPath.path ? this.currentDb?.casPath.path : "n/a"}`;
		this.basStatusBarWidget.show();
		this.ftdbStatusBarWidget.text = `FTDB: ${this.currentDb?.ftPath?.path ? this.currentDb?.ftPath.path : "n/a"}`;
		this.ftdbStatusBarWidget.show();
	}

	generateItems(pickType: string): ItemMap {
		let itemMap: ItemMap = {};
		let dbs: DBInfo[] = [];
		if (pickType === this.pickBASCommand) {
			itemMap = {
				"Add BAS server URL": {
					funct: this.showBASUrlInputBox,
					dbinfo: undefined,
				},
				"Select local BAS db file": {
					funct: this.showBASPicker,
					dbinfo: undefined,
				},
			};
			dbs = this.s.basDBs;
		}
		if (pickType === this.pickFTDBCommand) {
			itemMap = {
				"Add FTDB server URL": {
					funct: this.showFTDBUrlInputBox,
					dbinfo: undefined,
				},
				"Select local FTDB file": {
					funct: this.showFTDBPicker,
					dbinfo: undefined,
				},
			};
			dbs = this.s.FTDBs;
		}
		for (let index = 0; index < dbs.length; index++) {
			const e: DBInfo = dbs[index];
			itemMap[e.path] = { funct: this.selectOption, dbinfo: e };
		}
		return itemMap;
	}

	async showQuickPick(pickType: string): Promise<void> {
		const itemsMap = this.generateItems(pickType);
		const quickPick = vscode.window.createQuickPick();
		if (pickType === this.pickBASCommand) {
			quickPick.title = "BAS Database configuration";
		}
		if (pickType === this.pickFTDBCommand) {
			quickPick.title = "FTDB Database configuration";
		}
		quickPick.ignoreFocusOut = true;
		quickPick.canSelectMany = false;

		quickPick.items = Object.entries(itemsMap).map(([label, { dbinfo }]) => ({
			label,
			buttons:
				this.s.useRemoteBase && dbinfo?.scheme === "bas_file"
					? [
							this.ctx.globalState.get(
								`cas.server.useRemoteFor.${dbinfo.path}`,
								true,
							)
								? {
										iconPath: new vscode.ThemeIcon("device-desktop"),
										tooltip: "Use a local server for this path",
									}
								: {
										iconPath: new vscode.ThemeIcon("server-environment"),
										tooltip: "Try to find this path on remote",
									},
						]
					: [],
		}));
		quickPick.onDidTriggerItemButton(async (event) => {
			const x = itemsMap[event.item.label];
			if (x.dbinfo) {
				this.ctx.globalState.update(
					`cas.server.useRemoteFor.${x.dbinfo.path}`,
					event.button.tooltip !== "Use a local server for this path",
				);
				await x
					.funct(x.dbinfo, this)
					.catch((e) => this.logger.error`Error handling button click: ${e}`);
				quickPick.dispose();
			}
		});
		quickPick.onDidChangeSelection(async (selection) => {
			if (selection[0]) {
				const x = itemsMap[selection[0].label];
				await x
					.funct(x.dbinfo!, this)
					.catch((e) => this.logger.error`Error handling selection: ${e}`);

				quickPick.dispose();
			}
		});
		quickPick.onDidHide(() => quickPick.dispose());
		quickPick.show();
	}

	async showBASUrlInputBox(
		_dbInfo: DBInfo,
		instance: DBProvider,
	): Promise<void> {
		const result = await vscode.window.showInputBox({
			value: "",
			placeHolder: "Enter BAS server URL",
			validateInput: (text) => {
				return text.startsWith("http") ? null : "Wrong URL";
			},
		});
		if (result) {
			const dbInfo: DBInfo = { path: result, scheme: "bas_url" };
			this.logger.debug`BAS URL input: ${JSON.stringify(dbInfo)}`;
			let dbs: DBInfo[] | undefined = instance.s.basDBs;
			dbs?.push(dbInfo);
			dbs = Object.values(
				dbs?.reduce((out, i) => ({ ...out, [i.path]: i }), {}) ?? {},
			);
			await instance.s.set("basDatabases", dbs);
			await instance.switchDB(dbInfo);
		}
	}

	async showFTDBUrlInputBox(
		_dbInfo: DBInfo,
		instance: DBProvider,
	): Promise<void> {
		const result = await vscode.window.showInputBox({
			value: "",
			placeHolder: "Enter FTDB server URL",
			validateInput: (text) => {
				return text.startsWith("http") ? null : "Wrong URL";
			},
		});
		if (result) {
			const dbInfo: DBInfo = { path: result, scheme: "ftdb_url" };
			this.logger.debug`FTDB URL input: ${JSON.stringify(dbInfo)}`;
			let dbs: DBInfo[] | undefined = instance.s.FTDBs;
			dbs?.push(dbInfo);
			dbs = Object.values(
				dbs?.reduce((out, i) => ({ ...out, [i.path]: i }), {}) ?? {},
			);
			await instance.s.set("ftdbDatabases", dbs);
			await instance.switchDB(dbInfo);
		}
	}

	async showBASPicker(_dbInfo: DBInfo, instance: DBProvider): Promise<void> {
		const result = await vscode.window
			.showOpenDialog({
				canSelectFiles: true,
				canSelectMany: false,
				canSelectFolders: false,
				title: "Select .nfsdb.img file ...",
				filters: { "BAS database": ["img"] },
			})
			.then((data) => {
				return data?.pop();
			});

		if (result) {
			const dbInfo: DBInfo = { path: result.fsPath, scheme: "bas_file" };
			this.logger.debug`BAS picker selection: ${JSON.stringify(dbInfo)}`;
			let dbs: DBInfo[] = instance.s.basDBs;
			dbs.push(dbInfo);
			dbs = Object.values(
				dbs.reduce((out, i) => ({ ...out, [i.path]: i }), {}),
			);
			// TODO: remove the compat replacement in couple of version
			await instance.s.set(
				"basDatabases",
				dbs.map((db) => ({ ...db, scheme: db?.scheme?.replace("cas", "bas") })),
			);
			await instance.s.set("basDatabase", dbInfo);
			await instance.switchDB(dbInfo);
		}
	}

	async showFTDBPicker(_dbInfo: DBInfo, instance: DBProvider): Promise<void> {
		const result = await vscode.window
			.showOpenDialog({
				canSelectFiles: true,
				canSelectMany: false,
				canSelectFolders: false,
				title: "Select ftdb  img file ...",
				filters: { "FTDB database": ["img"] },
			})
			.then((data) => {
				return data?.pop();
			});

		if (result) {
			const dbInfo: DBInfo = { path: result.fsPath, scheme: "ftdb_file" };
			this.logger.debug`FTDB picker selection: ${JSON.stringify(dbInfo)}`;
			let dbs: DBInfo[] = instance.s.FTDBs;
			dbs.push(dbInfo);
			dbs = Object.values(
				dbs.reduce((out, i) => ({ ...out, [i.path]: i }), {}),
			);
			await instance.s.set("ftdbDatabases", dbs);
			await instance.s.set("ftdbDatabase", dbInfo);
			await instance.switchDB(dbInfo);
		}
	}

	async selectOption(dbInfo: DBInfo, instance: DBProvider): Promise<void> {
		this.logger.debug`Selected option: ${JSON.stringify(dbInfo)}`;

		// compatibility with previous setting names
		// TODO: remove in a couple versions
		dbInfo.scheme = dbInfo.scheme.replace("cas", "bas") as DBInfo["scheme"];
		if (dbInfo?.scheme === "bas_file" || dbInfo?.scheme === "bas_url") {
			await instance.currentDb?.stop();
		}
		await instance.switchDB(dbInfo);
	}
	// region DBProvider.switchDB
	async switchDB(dbInfo: DBInfo) {
		this.logger.debug`Switching DB: ${JSON.stringify(dbInfo)}`;
		if (dbInfo) {
			// compatibility with previous scheme names
			// TODO: remove in a couple versions
			dbInfo.scheme = dbInfo.scheme.replace("cas", "bas") as DBInfo["scheme"];
			if (dbInfo.scheme === "bas_file" || dbInfo.scheme === "bas_url") {
				this.logger.debug`Switching to BAS DB: ${dbInfo.path}`;
				this.s.set("basDatabase", dbInfo);
				await this.setCurrentDb(dbInfo);
				this.basStatusBarWidget.text = `BAS: ${this.currentDb?.casPath.path}`;
				this.basStatusBarWidget.show();
				this._onDBChange.fire(dbInfo);
			} else if (
				this.currentDb &&
				(dbInfo?.scheme === "ftdb_file" || dbInfo?.scheme === "ftdb_url")
			) {
				this.currentDb.ftPath = dbInfo;
				this.logger.debug`Switching to FTDB: ${this.currentDb.ftPath.path}`;
				this.currentDb.switchFtdb();
				this.ftdbStatusBarWidget.text = `FTDB: ${this.currentDb.ftPath.path}`;
				this.ftdbStatusBarWidget.show();
				this._onDBChange.fire(dbInfo);
			}
		}
	}

	async checkRemoteBase(info: BASDBInfo): Promise<BASDBInfo> {
		if (
			!this.s.useRemoteBase ||
			!this.s.remoteBase ||
			info.scheme !== "bas_file"
		) {
			return info;
		}
		const statusReq = await http.get(
			new URL("status", withTrailingSlash(this.s.remoteBase)),
		);
		const status = await statusReq.json<ServerStatus>();
		for (const [server, models] of Object.entries(status)) {
			for (const [model, data] of Object.entries<ModelStatus>(models)) {
				if (
					[data.db_dir, data.deps_path, data.nfsdb_path, data.loaded_nfsdb]
						.filter((p) => typeof p === "string")
						.map(normalize)
						.includes(info.path)
				) {
					const newPath = new URL(model, withTrailingSlash(server)).toString();
					this.ctx.globalState.update(
						`cas.server.useRemoteFor.${info.path}`,
						true,
					);
					return {
						path: newPath,
						scheme: "bas_url",
					};
				}
			}
		}
		return info;
	}

	private setDbInProgress = Promise.resolve();
	private async setCurrentDb(
		newDb: DBInfo | EmptyDBInfo,
		ftDb?: DBInfo | EmptyDBInfo,
	) {
		await this.setDbInProgress;
		const { promise: progressPromise, resolve } = Promise.withResolvers<void>();
		this.setDbInProgress = progressPromise;
		const selectedFTDB = ftDb?.path ? ftDb : this.currentDb?.ftPath;
		try {
			this.currentDb?.dispose();
			if (!newDb || !newDb.scheme) {
				return;
			}
			if (
				newDb.scheme.startsWith("bas") &&
				this.ctx.globalState.get(`cas.server.useRemoteFor.${newDb.path}`, true)
			) {
				newDb = await this.checkRemoteBase(newDb as BASDBInfo).catch(
					() => newDb,
				);
			}
			switch (newDb?.scheme) {
				case "bas_file":
					this.currentDb = new LocalServerCASDatabase(
						newDb,
						selectedFTDB,
						this.s,
					);
					break;
				case "bas_url":
					this.currentDb = new RemoteCASDatabase(newDb, selectedFTDB, this.s);
					break;
				default:
					return;
			}
			if (this.#dbSubscriptionIndex) {
				// replace db subscription with the new object
				this.ctx.subscriptions.splice(
					this.#dbSubscriptionIndex,
					1,
					this.currentDb,
				);
			} else {
				this.#dbSubscriptionIndex = this.ctx.subscriptions.length;
				this.ctx.subscriptions.push(this.currentDb);
			}
		} finally {
			resolve();
		}
	}

	public getDB(): CASDatabase | undefined {
		return this.currentDb;
	}
}
