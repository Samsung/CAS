import { toJsonSchema } from "@valibot/to-json-schema";
import { ManifestSchema } from "../dist/index.js"
import { writeFile } from "fs/promises";
import { join } from "path";
console.warn = () => {};
const schema = toJsonSchema(ManifestSchema, {force: true, errorMode: "warn"});

await writeFile(join(import.meta.dirname, "../dist/schema.json"), JSON.stringify(schema, null, 2))