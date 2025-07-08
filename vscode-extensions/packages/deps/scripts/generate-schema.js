import { toJsonSchema } from "@valibot/to-json-schema";
import { depsSchema } from "../dist/index.js"
import { writeFile } from "fs/promises";
import { join } from "path";
console.warn = () => { };
const schema = toJsonSchema(depsSchema, { errorMode: "warn" });

await writeFile(join(import.meta.dirname, "../dist/schema.json"), JSON.stringify(schema, null, 2))