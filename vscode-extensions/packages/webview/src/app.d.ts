import type { ParaglideLocals } from "@inlang/paraglide-js";
import type { AvailableLanguageTag } from "../../lib/paraglide/runtime";
// See https://kit.svelte.dev/docs/types#app
// for information about these interfaces
declare global {
	namespace App {
		// interface Error {}
		interface Locals {
			paraglide: ParaglideLocals<AvailableLanguageTag>;
		}
		// interface PageData {}
		// interface PageState {}
		// interface Platform {}
	}
}

export {};
