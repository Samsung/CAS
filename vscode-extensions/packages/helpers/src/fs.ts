import type { PathLike } from "node:fs";
import { access } from "node:fs/promises";
import { homedir } from "node:os";
import { isAbsolute, relative } from "node:path";
export async function testAccess(path: PathLike, mode?: number) {
	return access(path, mode)
		.then(() => true)
		.catch(() => false);
}

export function withLeadingSlash(path?: string) {
	return path?.startsWith("/") ? path : `/${path ?? ""}`;
}

export function withoutLeadingSlash(path?: string) {
	return path?.startsWith("/") ? path.slice(1) : (path ?? "");
}

export function withTrailingSlash(path?: string) {
	return path?.endsWith("/") ? path : `${path ?? ""}/`;
}
export function resolveHome(path: string): string;
export function resolveHome(path?: string) {
	return path?.startsWith("~") ? path.replace("~", homedir()) : path;
}

export function isSubpath(parent: string, child: string) {
	const relPath = relative(parent, child);
	return !relPath.startsWith("..") && !isAbsolute(relPath);
}
