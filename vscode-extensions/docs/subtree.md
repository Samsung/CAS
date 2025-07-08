# Repository setup

The development of vscode extensions is done in a separate repository which is then merged into CAS reposiotry using `git subtree`. The preferred flow would be as follows:

1. Rebase the cas branch in the vscode-extensions repository onto main 
    ```bash
    git checkout cas
    git rebase main
    ```

2. Add the vscode extensions repository as a remote (optional, you can just replace uses of `vscode-extensions-origin` with the actual git URL in commands below)
    ```bash
    git remote add vscode-extensions-origin <url>
    ```
3. Pull the subtree
    ```bash
    git subtree pull --prefix vscode-extensions vscode-extensions-origin cas --squash
    ```
4. Make changes to the subtree
    If there are any other CAS-specific changes you need to make (e.g. adding documentation) they're generally kept in the cas branch, so after making them you can just push them via subtree:
    ```bash
    git subtree push --prefix vscode-extensions vscode-extensions-origin cas
    ```