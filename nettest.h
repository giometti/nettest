#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "misc.h"

#define NETTEST_VERSION		__NETTEST_VERSION__
#define NETTEST_UDP_PORT	5000
#define NETTEST_PACKET_SIZE	1000

#define NETTEST_CMD_NONE	0
#define NETTEST_CMD_START	1
#define NETTEST_CMD_STOP	2
#define NETTEST_MODE_NONE 0
#define NETTEST_MODE_ACK  1
struct data_packet {
	unsigned char command;
	unsigned char mode;
	unsigned int period_ms;
	unsigned int pkt_num;
	char filler[1500];
};
