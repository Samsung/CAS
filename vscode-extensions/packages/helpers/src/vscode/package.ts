import { writeFile } from "node:fs/promises";

export type AnyJSONSchema = Record<string, any>;
export async function addConfigurationSchema(
	packageJSON: any,
	additionalSchemas: Record<string, AnyJSONSchema>,
	indent: undefined | "\t" | number = "\t",
) {
	for (const [key, schema] of Object.entries(additionalSchemas)) {
		packageJSON.contributes.configuration.properties[key] = schema;
	}
	await writeFile("./package.json", JSON.stringify(packageJSON, null, indent));
}
