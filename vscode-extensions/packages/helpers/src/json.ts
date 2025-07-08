import { ReadableStream } from "node:stream/web";
import { chain } from "stream-chain";
import { connectTo as asmConnectTo } from "stream-json/Assembler";
import { parser } from "stream-json/Parser";

export function parseStream<T>(stream: ReadableStream<string>) {
	const pipeline = chain([stream, parser()]);
	const asm = asmConnectTo(pipeline);
	return new Promise<T>((resolve) =>
		asm.on("done", (asm) => {
			resolve(asm.current);
		}),
	);
}
