= BAS database functionality

== Server selection

There are two supported ways of using BAS in the plugin:
- remote server
- local server (using BAS database files stored locally)

You can select a BAS database by clicking the definition in the status bar (`BAS: ...`) or running the `CAS: BAS Database Provider Settings` and picking one of the options.

The provider configuration consists of saved databases and two special options:
- Add BAS server URL - allows you to add a remote server by inputting an URL
- Select local BAS db file - allows you to pick a file for local use

There is a special case for local database files: if you have a `Remote Base` setting configured in your extension settings, the extension will check on the server under that URL for an existing BAS database matching the path you gave. If it finds one, it will use the remote database instead of the local one.

If you want to use a local database even when the remote one exists, you can either disabled the `Use Remote Base` setting, or simply open the provider dialog again and select the `use a local server for this path` button (Desktop PC icon) - this will force the extension to use the local database instead.
