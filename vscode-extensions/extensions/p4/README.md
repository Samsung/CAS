# cas-p4

Create and synchronize new P4 workspaces from CAS-generated file dependencies.


## Requirements

- ensure you have a working `p4` CLI client configured, you can downlaod it from [Perforce's site](https://www.perforce.com/downloads/helix-command-line-client-p4)

## Extension Settings

Required settings - make sure they are correct for your system:

* `cas-p4.mappingTemplatePath` path to a perforce mapping template JSON - a list of View-style path mappings inside a `template_mapping` key 
* `cas-p4.p4Path` - path to the p4 executable. Change if `p4` is not in your `PATH`.

## Development

This extension uses `pnpm`, you can [install it yourself](https://pnpm.io/installation) or use NodeJS corepack (run `corepack enable`).

#### Installing dependencies

```bash
pnpm install
```

#### Compilation

When opened in vscode, a compilation watch task should automatically start. You can also start it manually with `pnpm watch` or run a single-time compilation with `pnpm compile`

#### Tests

Currently this plugin doesn't have any working automated tests, but once they're added you should be able to run them via `pnpm test`. In the meantime `pnpm lint` and `pnpm check-types` can be used to help look for issues.

For manual testing you can use vscode's debugging functionality. There is a configuration set up for launching a new debug instance with this extension enabled already.