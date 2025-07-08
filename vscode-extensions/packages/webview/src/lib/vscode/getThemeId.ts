const themeMap = {
	Andromeeda: "andromeeda",
	"Aurora X": "aurora-x",
	"Ayu Dark": "ayu-dark",
	"Catppuccin Frappé": "catppuccin-frappe",
	"Catppuccin Latte": "catppuccin-latte",
	"Catppuccin Macchiato": "catppuccin-macchiato",
	"Catppuccin Mocha": "catppuccin-mocha",
	"Dark Plus": "dark-plus",
	"Dark+": "dark-plus",
	"Dracula Theme": "dracula",
	"Dracula Theme Soft": "dracula-soft",
	"GitHub Dark": "github-dark",
	"GitHub Dark Default": "github-dark-default",
	"GitHub Dark Dimmed": "github-dark-dimmed",
	"GitHub Light": "github-light",
	"GitHub Light Default": "github-light-default",
	Houston: "houston",
	LaserWave: "laserwave",
	"Light Plus": "light-plus",
	"Light+": "light-plus",
	"Material Theme": "material-theme",
	"Material Theme Darker": "material-theme-darker",
	"Material Theme Lighter": "material-theme-lighter",
	"Material Theme Ocean": "material-theme-ocean",
	"Material Theme Palenight": "material-theme-palenight",
	"Min Dark": "min-dark",
	"Min Light": "min-light",
	Monokai: "monokai",
	"Night Owl": "night-owl",
	Nord: "nord",
	"One Dark Pro": "one-dark-pro",
	"One Light": "one-light",
	Poimandres: "poimandres",
	Red: "red",
	"Rosé Pine": "rose-pine",
	"Rosé Pine Dawn": "rose-pine-dawn",
	"Rosé Pine Moon": "rose-pine-moon",
	"Slack Dark": "slack-dark",
	"Slack Ochin": "slack-ochin",
	"Snazzy Light": "snazzy-light",
	"Solarized Dark": "solarized-dark",
	"Solarized Light": "solarized-light",
	"Synthwave '84": "synthwave-84",
	"Tokyo Night": "tokyo-night",
	Vesper: "vesper",
	"Vitesse Black": "vitesse-black",
	"Vitesse Dark": "vitesse-dark",
	"Vitesse Light": "vitesse-light",
	"vscode-light": "light-plus",
	"vscode-dark": "dark-plus",
} as const;

export function mapToThemeId(
	themeName?: string | null,
	mode?: string | null,
): string {
	themeName = themeName?.replace("Default ", "");
	if (themeName && themeName in themeMap) {
		return themeMap[themeName as keyof typeof themeMap];
	}
	if (mode && mode in themeMap) {
		return themeMap[mode as keyof typeof themeMap];
	}
	// default to dark mode
	return "dark-plus";
}
