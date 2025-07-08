import { open } from "node:fs/promises";
import { chain } from "stream-chain";
import { connectTo as asmConnectTo } from "stream-json/Assembler";
import { parser } from "stream-json/Parser";
import * as v from "valibot";
import { DepsSchema, depsSchema } from ".";

export async function readDeps(path: string): Promise<DepsSchema> {
	const file = await open(path, "r");
	try {
		const stat = await file.stat();
		// just use JSON.parse on files below 64MiB
		// (size chosen arbitrarily, absolute max would be 511MiB since that's the string length limti)
		if (stat.size < 64 * 1024 * 1024) {
			return v.parse(depsSchema, JSON.parse(await file.readFile("utf-8")));
		}
		const pipeline = chain([file.readableWebStream(), parser()]);
		const asm = asmConnectTo(pipeline);
		return new Promise<DepsSchema>((resolve) =>
			asm.on("done", (asm) => {
				resolve(v.parse(depsSchema, asm.current));
			}),
		);
	} finally {
		await file.close();
	}
}
