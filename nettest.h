#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <sys/ioctl.h>

#include "misc.h"

#define NETTEST_VERSION		__NETTEST_VERSION__
#define NETTEST_PERIOD_MS	1000
#define NETTEST_UDP_PORT	5000
#define NETTEST_ETH_P		0xabba
#define NETTEST_PACKET_SIZE	1000
#define NETTEST_FILLER_SIZE	1500

#define NETTEST_INFO_TYPE_UDP	1
#define NETTEST_INFO_TYPE_ETHERNET	2
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
		struct comm_ethernt_data_s {
			char *if_name;
			uint8_t raw_if_address[ETH_ALEN];
			struct sockaddr_ll raw_address;
			struct sockaddr_ll raw_peer_address;
		} eth;
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
		struct data_ethernet_packet_u {
			struct ether_header eth;
		} eth;
	} proto;
	unsigned char command;
	unsigned char mode;
	unsigned int period_ms;
	unsigned int pkt_num;
	char filler[NETTEST_FILLER_SIZE];
};

/* Get the index of an interface */
static int get_ifindex(int sock, char *name)
{
        struct ifreq ifr;
        int ret;

        memset(&ifr, 0, sizeof(struct ifreq));
        strncpy(ifr.ifr_name, name, IFNAMSIZ - 1);

        ret = ioctl(sock, SIOCGIFINDEX, &ifr);
        if (ret < 0)
                return -errno;

        return ifr.ifr_ifindex;
}

/* Get the MAC address of an interface */
static int get_ifaddr(int sock, char *name, uint8_t if_addr[ETH_ALEN])
{
        struct ifreq ifr;
        int ret;

        memset(&ifr, 0, sizeof(struct ifreq));
        strncpy(ifr.ifr_name, name, IFNAMSIZ - 1);

        ret = ioctl(sock, SIOCGIFHWADDR, &ifr);
        if (ret < 0)
                return -errno;

        memcpy(if_addr, ifr.ifr_hwaddr.sa_data, ETH_ALEN);
        return 0;
}

static int parse_mac(char *str, uint8_t data[])
{
        unsigned int d;
        int n = strlen(str);
        int i;
        char *ptr;
        int len;
        int ret;

        i = 0;
        while (n > 0) {
                ptr = strchr(str, ':');
                if (ptr)
                        len = ptr - str;
                else
                        len = strlen(str);
                n -= len + 1;

                if (len == 2 || len == 1) {
			ret = sscanf(str, "%x", &d);
			if (ret != 1)
                                return 0;
                        *data = (uint8_t) d;
                } else
                        return 0;

                data++;
                str = ptr + 1;

                if (++i > 6)
                        return 0;
        }

        return 1;
}

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

	case NETTEST_INFO_TYPE_ETHERNET:
		ret = parse_mac(address, comm->proto.eth.raw_address.sll_addr);
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

	case NETTEST_INFO_TYPE_ETHERNET:
		ret = asprintf(&s, "%s",
			ether_ntoa((struct ether_addr *) comm->proto.eth.raw_address.sll_addr));
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

        case NETTEST_INFO_TYPE_ETHERNET:
                ret = asprintf(&s, "%s",
                        ether_ntoa((struct ether_addr *) &comm->proto.eth.raw_peer_address.sll_addr));
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

        case NETTEST_INFO_TYPE_ETHERNET:
                return "Ethernet";

        default:
                err("unsupported communication protocol!");
                exit(EXIT_FAILURE);
        }
}
