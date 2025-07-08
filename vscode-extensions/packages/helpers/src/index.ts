// exports everything that's dependency-free
export * from "./array";
export * from "./fs";
export * from "./math";
export * from "./promise";
export * from "./text";

export async function sleep(ms: number) {
	return new Promise((resolve) => setTimeout(resolve, ms));
}
