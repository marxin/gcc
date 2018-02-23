# Please add "source /path/to/bash-autocomplete.sh" to your .bashrc to use this.

log()
{
  echo $1 >> /tmp/bash-completion.log
}

_gcc()
{
    local cur prev prev2 words cword argument prefix
    _init_completion || return
    _expand || return

    # extract also for situations like: -fsanitize=add
    if [[ $cword > 2 ]]; then
      prev2="${COMP_WORDS[$cword - 2]}"
    fi

    log "cur: '$cur', prev: '$prev': prev2: '$prev2' cword: '$cword'"

    # sample: -fsan
    if [[ "$cur" == -* ]]; then
      argument=$cur
    # sample: -fsanitize=
    elif [[ "$cur" == "=" && $prev == -* ]]; then
      argument=$prev$cur
      prefix=$prev$cur
    # sample: -fsanitize=add
    elif [[ "$prev" == "=" && $prev2 == -* ]]; then
      argument=$prev2$prev$cur
      prefix=$prev2$prev
    # sample: --param lto-
    elif [[ "$prev" == "--param" ]]; then
      argument="$prev $cur"
      prefix="$prev "
    fi

    log  "argument: '$argument', prefix: '$prefix'"

    if [[ "$argument" == "" ]]; then
      _filedir
    else
      # In situation like '-fsanitize=add' $cur is equal to last token.
      # Thus we need to strip the beginning of suggested option.
      flags=$( gcc --completion="$argument" 2>/dev/null | sed "s/^$prefix//")
      log "compgen: $flags"
      [[ "${flags: -1}" == '=' ]] && compopt -o nospace 2> /dev/null
      COMPREPLY=( $( compgen -W "$flags" -- "") )
    fi
}
complete -F _gcc gcc
