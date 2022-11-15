#!/bin/bash

# Source file for common definitions
. misc.sh.inc

#
# Commands
#

function do_setup () {
	set -e
	
	debug "creating vEthernets..."
	R ip link add veth0 type veth peer name veth1

	debug "adding namespace ns0..."
	R ip netns add ns0
	debug "moving interface veth0 to ns0..."
	R ip link set veth0 netns ns0
	debug "setting ip 192.168.22.1/24 to interface veth0..."
	R ip netns exec ns0 ip addr add 192.168.22.1/24 dev veth0
	R ip netns exec ns0 ip link set veth0 up
	
	debug "setting ip 192.168.22.2/24 to interface veth1..."
	R ip addr add 192.168.22.2/24 dev veth1
	R ip link set veth1 up

	debug "executing nettestd within ns0..."
	info "After nettestd starting you can do:\n\t../nettestc $nettest_debug -f 200 192.168.22.1"
	info "nettestd started with:\n\tip netns exec ns0 ../nettests $nettest_debug"
	nettest_debug="" ; [ $DEBUG -eq 1 ] && nettest_debug="-d"
	R ip netns exec ns0 ../nettests $nettest_debug
	
}

function do_destroy () {
	debug "killing nettestd within ns0..."
	R ip netns exec ns0 killall nettestd

	debug "setting interface veth1 down..."
	R ip link set veth1 down

	debug "setting interface veth0 down..."
	R ip netns exec ns0 ip link set veth0 down
	debug "removing interface veth0 from ns0..."
	R ip link set veth0 netns 1

	debug "removing namespace ns0..."
	R ip netns delete ns0

	debug "removing vEthernets..."
	R ip link del veth0
}

#
# Usage
#

function usage () {
	echo "usage: $NAME [-h | --help] [-d | --debug] [--dry-run] <COMMAND>" >&2
	echo "  where <COMMAND> can be:" >&2
	echo "    setup     - setup ip addresses for ping test" >&2
	echo "    destroy   - destroy whatever created with 'setup'" >&2
        exit 1
}

#
# Main
#

# Check command line
TEMP=$(getopt -o hd --long help,debug,dry-run -n $NAME -- "$@")
[ $? != 0 ] && exit 1
eval set -- "$TEMP"
while true ; do
        case "$1" in
	-h|--help)
                usage
                ;;

	-d|--debug)
                DEBUG=1
		shift
                ;;

	--dry-run)
                DRY_RUN=1
		shift
                ;;
        --)
                shift
                break
                ;;

        *)
                fatal "internal error!"
                ;;
        esac
done
[ $# -lt 1 ] && usage
eval cmd=$1

# Check for root user
if [ $EUID != 0 ]; then
        sudo env "PATH=$PATH" ionice -c 3 "$0" $TEMP
        exit $?
fi

case $cmd in
setup)
	do_setup
	;;
destroy)
	do_destroy
	;;
*)
	fatal "invalid sub command"
esac

exit 0
