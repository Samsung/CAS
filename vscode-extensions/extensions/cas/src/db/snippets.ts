import { commands, Event, EventEmitter, ExtensionContext } from "vscode";

const builtins: [string, string][] = [
	[
		"vmlinux + kos",
		"linked_modules --filter=[path=*/vmlinux,type=wc]and[path=*compressed*,type=wc,negate=true]or[path=*.ko,type=wc,exists=1] deps_for --filter=[exists=1,source_root=1]and[path=.*\\.o$|.*\\.o.d$|.*\\.a$|.*\\.so$|.*\\.so\\.\\d$|.*\\.ko$|.*\\/.git\\/.*,type=re,negate=1]",
	],
];
const internal: [string, string][] = [
	[
		"depsFilter",
		"--filter='[exists=1,source_root=1]and[path=.*\\.o$|.*\\.o.d$|.*\\.a$|.*\\.so$|.*\\.so\\.\\d$|.*\\.ko$|.*\\/.git\\/.*,type=re,negate=1]'",
	],
];

export const internalSnippets = new Map(internal);

export class Snippets extends Map<string, string> {
	ctx: ExtensionContext;
	#changeEmitter = new EventEmitter<this>();
	onchange: Event<this> = this.#changeEmitter.event;
	static instance?: Snippets = undefined;
	private constructor(ctx: ExtensionContext) {
		super(ctx.globalState.get<[string, string][]>("cas.snippets", builtins));
		this.ctx = ctx;

		ctx.subscriptions.push(
			commands.registerCommand(
				"cas.snippets.delete",
				(entry: { name: string }) => {
					this.delete(entry.name);
				},
			),
		);
	}
	static init(ctx: ExtensionContext): Snippets {
		if (this.instance) {
			return this.instance;
		}
		this.instance = new Snippets(ctx);
		return this.instance;
	}

	reorder(keys: string[]) {
		const sorted = [...super.entries()].sort(
			([a], [b]) => keys.indexOf(a) - keys.indexOf(b),
		);
		super.clear();
		for (const [key, value] of sorted) {
			super.set(key, value);
		}
		if (this.ctx) {
			this.ctx.globalState.update("cas.snippets", [...this.entries()]);
			this.#changeEmitter.fire(this);
		}
	}

	clear(): void {
		super.clear();
		if (this.ctx) {
			this.ctx.globalState.update("cas.snippets", [...this.entries()]);
			this.#changeEmitter.fire(this);
		}
	}
	delete(key: string): boolean {
		if (super.delete(key)) {
			if (this.ctx) {
				this.ctx.globalState.update("cas.snippets", [...this.entries()]);
				this.#changeEmitter.fire(this);
			}
			return true;
		}
		return false;
	}
	set(key: string, value: string): this {
		super.set(key, value);
		if (this.ctx) {
			this.ctx.globalState.update("cas.snippets", [...this.entries()]);
			this.#changeEmitter.fire(this);
		}
		return this;
	}
}
