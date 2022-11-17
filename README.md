# NetTest

This projects implements a client/server couple to test and analyze network connectivity and loops.

## Feature

The client generates periodic packets to the server which in turn displays
possible down times, duplicate packets, etc.

At the moment we can use both UDP or Ethernet packets with different payloads and generation frequencies.

## Compile

To compile the programs just use `make` as follow:

    $ make

You can use some variables to alter compilation such as:

* `DYNAMIC=n` to get statically linked programs.
* `CROSS_COMPILE=aarch64-linux-gnu-` (or any other prefix) to select a cross compiler.

## Basic usage

Once compiling is done you should get two programs: `nettestc` and `nettests`:

    $ nettestc -h
    usage: nettestc [-h | --help] [-d | --debug] [-t | --print-time]
                   [-v | --version]
                   [-p <port>] [-i | --use-ethernet <iface>]
                   [-s <size>] [-f <period>] [-n <packets>] [-a]  <addr>
      defaults are:
        - port is 5000
        - size is 1000 bytes for payload
        - period is 1000ms
    $ nettests -h
    usage: nettests [-h | --help] [-d | --debug] [-t | --print-time]
                   [-v | --version]
                   [-p <port>] [-m addr]
                   [-i | --use-ethernet <iface>]
      defaults are:
        - port is 5000

`nettestc` take an IP address or a MAC address and then starts sending periodic packets to that destination, while `nettests` waits until some packet arrives then it starts reporting possible duplicated or out-of-order packets or missed packets (in case of downtime).

### Examples

Here a simple UPD usage example:

    $ nettestc 192.168.32.25

    $ nettests

Here a simple Ethernet example:

    $ nettestc -i eth0 80:fa:5b:84:77:13

    $ nettests -i enp4s0f1

Note that for Ethernet you must specify the `-i` option argument!
