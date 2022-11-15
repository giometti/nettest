#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "misc.h"

#define NETTEST_VERSION		__NETTEST_VERSION__
#define NETTEST_PERIOD_MS	1000
#define NETTEST_UDP_PORT	5000
#define NETTEST_PACKET_SIZE	1000
#define NETTEST_FILLER_SIZE	1500

#define NETTEST_INFO_TYPE_UDP	1
struct comm_info_s {
	unsigned int type;
	size_t packet_size;
	unsigned int period_ms;
	unsigned int packets_num;
	bool use_ack;
	union comm_proto_u {
		struct comm_udp_data_s {
			struct sockaddr_in raw_address;
			unsigned int port;
			char *multicast_address;
			struct sockaddr_in raw_peer_address;
		} udp;
	} proto;
};

#define NETTEST_CMD_NONE	0
#define NETTEST_CMD_START	1
#define NETTEST_CMD_STOP	2
#define NETTEST_MODE_NONE 0
#define NETTEST_MODE_ACK  1
struct data_packet_s {
	union data_packet_u {
		struct data_udp_packet_u {
		} udp;
	} proto;
	unsigned char command;
	unsigned char mode;
	unsigned int period_ms;
	unsigned int pkt_num;
	char filler[NETTEST_FILLER_SIZE];
};

static inline void nettest_set_address(struct comm_info_s *comm,
					char *address)
{
	int ret;

        switch (comm->type) {
        case NETTEST_INFO_TYPE_UDP:
		ret = inet_aton(address, &comm->proto.udp.raw_address.sin_addr);
                err_if_exit(ret < 0, EXIT_FAILURE,
                                        "cannot convert address");
		break;

        default:
                err("unsupported communication protocol!");
                exit(EXIT_FAILURE);
        }
}

static inline char *nettest_get_address(struct comm_info_s *comm)
{
        char *s = NULL;
        int ret;

        switch (comm->type) {
        case NETTEST_INFO_TYPE_UDP:
                ret = asprintf(&s, "%s:%u",
                        inet_ntoa(comm->proto.udp.raw_address.sin_addr),
                        ntohs(comm->proto.udp.raw_address.sin_port));
                err_if_exit(ret < 0, EXIT_FAILURE,
                                        "cannot convert peer address");
                return s;

        default:
                err("unsupported communication protocol!");
                exit(EXIT_FAILURE);
        }
}

static inline char *nettest_get_peer_address(struct comm_info_s *comm)
{
	char *s = NULL;
	int ret;

        switch (comm->type) {
        case NETTEST_INFO_TYPE_UDP:
		ret = asprintf(&s, "%s:%u",
			inet_ntoa(comm->proto.udp.raw_peer_address.sin_addr),
			ntohs(comm->proto.udp.raw_peer_address.sin_port));
		err_if_exit(ret < 0, EXIT_FAILURE,
					"cannot convert peer address");
		return s;

        default:
                err("unsupported communication protocol!");
                exit(EXIT_FAILURE);
        }
}

static inline char *nettest_get_proto(struct comm_info_s *comm)
{
        switch (comm->type) {
        case NETTEST_INFO_TYPE_UDP:
                return "UDP";

        default:
                err("unsupported communication protocol!");
                exit(EXIT_FAILURE);
        }
}
