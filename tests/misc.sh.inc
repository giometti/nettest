#!/bin/bash
#
if [[ "${BASH_SOURCE[0]}" = "${0}" ]] ; then
        echo "Don't call me directly! Just source me" >&2
        exit 1
fi

__NC="\e[39m"
__GREEN="\e[32m"
__RED="\e[31m"
__YELLOW="\e[33m"
__MAGENTA="\e[36m"

# Abort mechanism
trap "exit 1" TERM
__PID=$$
function die() {
	kill -s TERM $__PID
}

# *****************************************************************************
# * Exported API
# *****************************************************************************

#
# Exported variables
#

NAME=$(basename $0)
DEBUG=0
DRY_RUN=0

NODES_NUM=4

#
# Exported functions
#

function fatal() {
	echo -ne $__RED >&2
        echo -ne "$NAME: $@" >&2
	echo -e $__NC >&2
	die
}

function warn() {
        echo -ne $__YELLOW
        echo -ne "$NAME: $@"
        echo -e $__NC
}

function info() {
	echo -ne $__GREEN
        echo -ne "$NAME: $@"
	echo -e $__NC
}

function debug() {
	[ $DEBUG -eq 0 ] && return
	echo -ne $__MAGENTA >&2
	echo -ne "$NAME: $@" >&2
	echo -e $__NC >&2
}

function check_notdef() {
	local v
        for v in $* ; do
                [ -z "${!v}" ] && fatal "$v variable is not defined!"
        done
}

function R() {
	if [ $DRY_RUN -eq 0 ] ; then
		$@
	else
		echo $@
	fi
}
