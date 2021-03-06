__crucible_commands="bootstrap dump-image test help ls-tests run version"

__crucible_bootstrap()
{
   COMPREPLY=($(compgen -W "--help --width --height $($1 ls-tests)" -- ${COMP_WORDS[COMP_CWORD]}))
}

__crucible_dump_image()
{
   COMPREPLY=($(compgen -o filenames -A file -W "--help" -- ${COMP_WORDS[COMP_CWORD]}))
}

__crucible_help()
{
    COMPREPLY=($(compgen -W "$__crucible_commands" -- ${COMP_WORDS[COMP_CWORD]}))
}

__crucible_ls_tests()
{
    COMPREPLY=($(compgen -W "--help" -- ${COMP_WORDS[COMP_CWORD]}))
}

__crucible_run()
{
   local flags="
      --help
      --fork
      --no-fork
      --separate-cleanup-threads
      --no-separate-cleanup-threads
      -I --isolation
      -j --jobs
      --dump
      --no-dump
      --no-cleanup
      --use-spir-v
      --junit-xml
      --device-id
   "

   COMPREPLY=($(compgen -W "$flags $($1 ls-tests)" -- ${COMP_WORDS[COMP_CWORD]}))
}

__crucible()
{
    local i c=1 command
    local crucible_options="--help --version"

    while [ $c -lt $COMP_CWORD ]; do
	i="${COMP_WORDS[c]}"
	case "$i" in
	    -*) ;;
	    *) command="$i"; break ;;
	esac
	c=$((++c))
    done

    if [ $c -eq $COMP_CWORD -a -z "$command" ]; then
	case "${COMP_WORDS[COMP_CWORD]}" in
	    --*=*) COMPREPLY=() ;;
	    -*) COMPREPLY=($(compgen -P "" -W "$crucible_options" -- ${COMP_WORDS[COMP_CWORD]})) ;;
	    *) COMPREPLY=($(compgen -P "" -W "$__crucible_commands $crucible_options" -- ${COMP_WORDS[COMP_CWORD]})) ;;
	esac
	return
    fi

    case "$command" in
	bootstrap) __crucible_bootstrap $1 ;;
	dump-image) __crucible_dump_image $1 ;;
	help) __crucible_help ;;
	ls-tests) __crucible_ls_tests $1 ;;
	run) __crucible_run $1 ;;
	*) COMPREPLY=() ;;
    esac
}

complete -F __crucible crucible ./bin/crucible
