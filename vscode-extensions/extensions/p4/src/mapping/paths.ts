import { sep } from "node:path";
import { join } from "node:path/posix";

/**
 * Find the longer common prefix between all given paths
 * @param paths any number of paths to find a common prefix for
 * @returns longest common prefix
 */
export function commonPathPrefix(...pathArgs: string[] | string[][]): string {
	const paths = pathArgs.flat();
	if (!paths.length) {
		return "";
	}
	const [first, ...remaining] = paths;
	// default to forward slash even on Windows if there are no backslashes
	const separator = first.includes(sep) ? sep : "/";
	const firstParts = first.split(separator);

	let maxPrefixLength = firstParts.length;
	for (const path of remaining) {
		const parts = path.split(separator);
		for (let i = 0; i < maxPrefixLength; i++) {
			if (firstParts[i] !== parts[i]) {
				maxPrefixLength = i;
			}
		}
	}
	// ensure that joining just the first part results in root path
	firstParts[0] ||= "/";
	const commonPrefix = join(...firstParts.slice(0, maxPrefixLength));
	return commonPrefix !== "." ? suffix(commonPrefix) : "";
}

/**
 * Transform an absolute path to just the subpath (e.g. from /android/kernel/drivers with /android as root to /kernel/drivers)
 * @param root starting path
 * @param path full path that's below the root
 * @returns just the subpath
 */
export function subPath(root: string, path: string) {
	return path.slice(root.length);
}

export function suffix(path: string) {
	return path.endsWith("/") ? path : `${path}/`;
}

export function toRelative(path: string) {
	return path.startsWith("/") ? path.slice(1) : path;
}
