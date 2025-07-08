// from https://github.com/oplik0/base91-deno
const lookup: string[] = [];
const revLookup: number[] = [];
const code =
	'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!#$%&()*+,./:;<=>?@[]^_`{|}~"';

for (let i = 0, len = code.length; i < len; ++i) {
	lookup[i] = code[i];
	revLookup[code.charCodeAt(i)] = i;
}
/**
 * Converts given data with base91 encoding
 * @param uint8 input to encode
 */
export function encode(uint8: Uint8Array): string {
	let output = "";
	let queue = 0,
		numbits = 0,
		value = 0;
	for (let i = 0, len = uint8.length; i < len; i++) {
		queue |= uint8[i] << numbits;
		numbits += 8;
		if (numbits > 13) {
			value = queue & 8191;
			if (value > 88) {
				queue >>= 13;
				numbits -= 13;
			} else {
				value = queue & 16383;
				queue >>= 14;
				numbits -= 14;
			}
			output += lookup[value % 91] + lookup[Math.trunc(value / 91)];
		}
	}
	if (numbits > 0) {
		output += lookup[queue % 91];
		if (numbits > 7 || queue > 90) {
			output += lookup[Math.trunc(queue / 91)];
		}
	}
	return output;
}
/**
 * Converts given base91 encoded data back to original Uint8Array
 * @param b91 input to decode
 */
export function decode(b91: string): Uint8Array {
	const output: number[] = [];
	let queue = 0,
		numbits = 0,
		value = -1,
		d = 0;
	for (let i = 0, len = b91.length; i < len; i++) {
		d = revLookup[b91.charCodeAt(i)];
		if (d === undefined) {
			continue;
		}
		if (value === -1) {
			value = d;
		} else {
			value += d * 91;
			queue |= value << numbits;
			numbits += (value & 8191) > 88 ? 13 : 14;
			do {
				output.push(queue);
				queue >>= 8;
				numbits -= 8;
			} while (numbits > 7);
			value = -1;
		}
	}
	if (value !== -1) {
		output.push(queue | (value << numbits));
	}
	return new Uint8Array(output);
}
/*
license from source:
MIT License

Copyright (c) 2020 Opliko

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
