#/usr/bin/env bash
_cas_completions() {
    COMPREPLY=($(compgen  -W "`cas bash_complete ${COMP_WORDS[@]}`" -- "$2" ))
}

complete -F _cas_completions cas