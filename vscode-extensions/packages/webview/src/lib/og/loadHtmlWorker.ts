export { loadHtml } from "./loadHtmlHelpers";

import { expose } from "comlink";
import { loadHtml } from "./loadHtmlHelpers";

expose({
	loadHtml,
});
