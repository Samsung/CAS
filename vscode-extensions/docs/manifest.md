# CAS plugin manifest format

CAS VSCode plugin is meant to largely operate by importing settings from "manifest files". At its core, a manifest file is a VSCode workspace file (a JSON, typically with a `.code-workspace` file extension) that contains a `cas.manifest` setting key. As such the "envelope" of the format typically looks as follows:
```json
{
    "folders": [
    {
      "path": "."
    }
  ],
  "settings": {
    "cas.manifest": {
        // ...
    }
  }
}
```

## manifest settings
All keys under `cas.manifest` are optional. In rough order of importance the available keys are as follows:
- `basProvider` - where to connect for BAS functionality
  - `type` - `file` or `url` - `file` starts local server with a give nfsdb path while `url` links to an existing server URL 
  - `path` - path to `.nfsdb` or URL to a running server
- `sourceRepo` - where to get the files from
  - `type` - `local`, `sftp`
  - for `"type": "local"`:
    - `sourceRoot` - root path the files should be linked from, if different than in BAS database
  - for `"type": "sftp"`
    - `sourceRoot` - root path the files should be downloaded from, if different than in BAS database
    - `hostname` - hostname, IP or alias from `.ssh_config`. If you're using an already configured host fron your SSH config, you don't need to define the other connection properties here
    - `port` - defaults to 22
    - `username` - what user to connect as
    - `keyFile` - path to private key file to use when connecting, password auth is not supported.
  - `opengrok` - OpenGrok setup
    - `url` - link to the OG instance
    - `apiKey` - API key, required for most OG functionality
  - `remote` - use a remote server for the actual workspace instead of your local machine. Typically allows for using the fastest `local` `sourceRepo` type (without having to download the whole build) and simply just running on a more powerful machine
    - `hostname` - hostname, IP or alias from `.ssh_config`. If you're using an already configured host fron your SSH config, you don't need to define the other connection properties here
    - `port` - defaults to 22
    - `username` - what user to connect as
    - `keyFile` - path to private key file to use when connecting, password auth is not supported.
  - `snippets` - pre-set list of database client shortcuts
    - `name` - the name displayed to the user
    - `command` - CAS client command to run
  - `ftdbProvider` - currently unused, reserved for future deeper ftdb integration. Format identical to `basProvider`
  - `version` - number or semver-like string, currently unused (and entirely optional), added to have a way of denoting future breaking versions of the manifest format

## full general schema

A rough more json-like schema from [#69](https://github.sec.samsung.net/CO7-SRPOL-Mobile-Security/cas-vscode-extension/issues/69#issuecomment-2983343)
```json
{
    "folders": [{"path": <string>}],    // defines the root workspace folder for the project
                                        // This is the directory where the project source code will be setup
                                        // along with the additional project related files (like compilation database)
                                        // This folder is used both locally and during the remote connection
    "settings": {
        "cas.manifest": {
            "basProvider": {    // required=False
                                // If not present all BAS related features are disabled
                "path": <string>,
                "type": <string:"url">|<string:"file">
            },
            "ftdbProvider":  {	// required=False
            					// If not present all FTDB related features are disabled
                "path": <string>,
                "type": <string:"url">|<string:"file">
            },
            "remote": { // required=False
                        // Remote connection can be used with any source code provider
                        // (the source will be instantiated/used in the remote location)
                        // If this key is not present the remote connection is not used
                "hostname": <string>,
                "username": <string>, // optional
                "port": <int>,	// optional
                "keyFile": <string> // optional
            },
            "sourceRepo": { // required=False
                // If not used the "local" repo type is assumed
                "type": <string:"local">|<string:"sftp">,
                // <local specific keys>
                "sourceRoot": <string>, // Only required in "local" and "sftp" repo type to point to existing source directory
                                        // When using other source type the source directory is created in workspace directory
                                        // If this key is not present the source root is taken from the BAS database
                                        // (if BAS related features are disabled then no source directory will be created)
                // <sftp specific keys>
                "sourceRoot": <string>,
                "hostname": <string>,
                "username": <string>, // optional
                "port": <int>,	// optional
                "keyFile": <string> // optional
            },
            "opengrok": {   // required=False
                "url": <string/url>,
                "apiKey": <string>
            }
        }
    }
}
```

Additionally you can generate a full JSON schema by running `Developer: Show Manifest JSON Schema` command from VSCode.

You may also notice that when editing such `.code-workspace` file VSCode will suggest the keys, which utilizes the same JSON schema as can be generated with the command above.

## Variables

String properties support VSCode variable (predefined and environment variables), see [VSCode documentation](https://code.visualstudio.com/docs/editor/variables-reference) for more details on what's available.