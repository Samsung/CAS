import { addConfigurationSchema } from "@cas/helpers/vscode/package.js"

import packageJSON from "../package.json" with { type: "json" }
import manifestSchema from "@cas/manifest/schema.json" with { type: "json" }
console.log("synchronizing configuration schemas...")
await addConfigurationSchema(packageJSON, {"cas.manifest": manifestSchema})
console.log("done synchronizing configuration schemas")