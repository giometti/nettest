#include <getopt.h>
#include "nettest.h"

int __debug_level;
int __add_time;

static int prompt_n;
static char prompt_symbol[] = { '|', '/', '-', '\\' };

static int open_socket(struct comm_info_s *comm)
{
	int s;
	struct sockaddr_in addr;
	struct ip_mreq mc_request;
	int ret;

	switch (comm->type) {
	case NETTEST_INFO_TYPE_UDP:
		dbg("selected communication protocol is UDP");

		s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	        err_if_exit(s < 0, EXIT_FAILURE, "unable to open socket: %m");

		if (comm->proto.udp.multicast_address) {
			/* Construct a IGMP join request structure */
			mc_request.imr_multiaddr.s_addr =
				inet_addr(comm->proto.udp.multicast_address);
			mc_request.imr_interface.s_addr = htonl(INADDR_ANY);

			/* Set ADD MEMBERSHIP message */
			ret = setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                                (void *)&mc_request, sizeof(mc_request));
			err_if_exit(ret < 0, EXIT_FAILURE,
					"cannot add membership: %m");
                }

		/* Bind the socket */
		addr.sin_family = AF_INET;
		addr.sin_port = htons(comm->proto.udp.port);
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
		ret = bind(s, (struct sockaddr *) &addr, sizeof(addr));
		err_if_exit(ret < 0, EXIT_FAILURE, "cannot bind socket: %m");
		break;

        case NETTEST_INFO_TYPE_ETHERNET:
                dbg("selected communication protocol is Ethernet");

                s = socket(AF_PACKET, SOCK_RAW, htons(NETTEST_ETH_P));
                err_if_exit(s < 0, EXIT_FAILURE, "unable to open socket: %m");

                /* Complete the Ethernet sockaddr_ll data */
                ret = get_ifindex(s, comm->proto.eth.if_name);
                err_if_exit(ret < 0, EXIT_FAILURE,
                                        "cannot get interface index");
                comm->proto.eth.raw_address.sll_ifindex = ret;
                comm->proto.eth.raw_address.sll_family = AF_PACKET;
                comm->proto.eth.raw_address.sll_protocol = htons(NETTEST_ETH_P);
                comm->proto.eth.raw_address.sll_halen = ETH_ALEN;

                ret = bind(s, (struct sockaddr *) &comm->proto.eth.raw_address,
                                sizeof(comm->proto.eth.raw_address));
                err_if_exit(ret < 0, EXIT_FAILURE,
                                        "cannot bind interface");

                /* Get the MAC address of the interface to recv from */
                ret = get_ifaddr(s, comm->proto.eth.if_name,
                                        comm->proto.eth.raw_if_address);
                err_if_exit(ret < 0, EXIT_FAILURE,
                                        "cannot get MAC address: %m");

                break;

        default:
                err("unsupported communication protocol!");
                exit(EXIT_FAILURE);
        }

        return s;
}

static ssize_t send_data(int s, struct comm_info_s *comm,
                                struct data_packet_s *pkt, size_t len)
{
	socklen_t addr_len;

        switch (comm->type) {
        case NETTEST_INFO_TYPE_UDP:
		addr_len = sizeof(comm->proto.udp.raw_peer_address);
                return sendto(s, pkt, len, 0,
                       (struct sockaddr *) &comm->proto.udp.raw_peer_address,
				addr_len);

        case NETTEST_INFO_TYPE_ETHERNET:
		addr_len = sizeof(comm->proto.eth.raw_peer_address);
                memcpy(pkt->proto.eth.eth.ether_shost,
                                comm->proto.eth.raw_if_address, ETH_ALEN);
                memcpy(pkt->proto.eth.eth.ether_dhost,
                                comm->proto.eth.raw_peer_address.sll_addr,
								ETH_ALEN);
                pkt->proto.eth.eth.ether_type = htons(0xabba);

                return sendto(s, pkt, len, 0, NULL, 0);

        default:
                err("unsupported communication protocol!");
                exit(EXIT_FAILURE);
        }
}

static ssize_t recv_data(int s, struct comm_info_s *comm,
                                struct data_packet_s *pkt, size_t len)
{
	socklen_t addr_len;

        switch (comm->type) {
        case NETTEST_INFO_TYPE_UDP:
		addr_len = sizeof(comm->proto.udp.raw_peer_address);
                return recvfrom(s, pkt, len, 0,
                       (struct sockaddr *) &comm->proto.udp.raw_peer_address,
				& addr_len);

	case NETTEST_INFO_TYPE_ETHERNET:
		addr_len = sizeof(comm->proto.eth.raw_peer_address);
		return recvfrom(s, pkt, len, 0,
			(struct sockaddr *) &comm->proto.eth.raw_peer_address,
                                & addr_len);

        default:
                err("unsupported communication protocol!");
                exit(EXIT_FAILURE);
        }
}

static void mainloop(int s, struct comm_info_s *comm)
{
	int receive = 1;
	unsigned int prev_pkt_num;
	static struct data_packet_s pkt_recv;
	struct timeval t1, t2, t3;
	long delta_s, delta_u, delta_point;
	unsigned long elapsed_us;
	unsigned long long elapsed_avg_us;
	ssize_t nrecv, nsent;
	char *str;

	prev_pkt_num = 0;
	elapsed_avg_us = 0;
	t1.tv_sec = t2.tv_sec = t3.tv_sec = 0;
	t1.tv_usec = t2.tv_usec = t3.tv_usec = 0;
	delta_s = delta_u = delta_point = 0;

	while (receive) {
		nrecv = recv_data(s, comm, &pkt_recv, sizeof(pkt_recv));
		err_if_exit(nrecv < 0, EXIT_FAILURE,
					"cannot receive packet: %m");

		/*
		 * Get current time and compute the time difference
		 * from previus packet
		 */
		gettimeofday(&t2, NULL);
		if (t1.tv_sec) {
			delta_s = t2.tv_sec - t1.tv_sec;
			delta_u = t2.tv_usec - t1.tv_usec;
		}

		if (pkt_recv.command == NETTEST_CMD_START) {
			info("new transmission detected, resetting counters");

			if (pkt_recv.period_ms)
				info("frequency announced is 1 packet "
					"every %dms", pkt_recv.period_ms);
			else
				info("frequency announced is at wire speed");

			info("client address is %s",
				str = nettest_get_peer_address(comm));
			free(str);

			prev_pkt_num = 0;
			elapsed_avg_us = 0;
			t1.tv_usec = t3.tv_usec = 0;
			t1.tv_sec = t3.tv_sec = 0;
		} else if (pkt_recv.command == NETTEST_CMD_STOP)
			info("transmission completed, "
				"received %u packets (avg ipt %lluus)",
				prev_pkt_num, elapsed_avg_us);

		/*
		 * Print nice prompt to easily see what's happening,
		 * (if debugging is disabled):
		 * - print rotating symbols continuosly
		 * - print a 'dot' every second
		 */
		printf("\b%c", prompt_symbol[prompt_n]);
		if (__debug_level == 0 && (t2.tv_sec - t3.tv_sec) > 1) {
			t3 = t2;
			printf("\b.%c", prompt_symbol[prompt_n]);
		}
		prompt_n = (prompt_n + 1) % ARRAY_SIZE(prompt_symbol);
		/* Flush stdout and save current time for next loop */
		fflush(stdout);
		t1 = t2;

		/* Calculate the inter packet time */
		elapsed_us = (delta_s) * 1000000 + delta_u;

		/* Update the averge interpacket time */
		if (elapsed_avg_us)
			elapsed_avg_us = (elapsed_avg_us + elapsed_us) / 2;
		else
			elapsed_avg_us = elapsed_us;
		dbg("recv pkt=%u/%u size=%ld ipt=%luus",
		     pkt_recv.pkt_num, prev_pkt_num, nrecv, elapsed_us);

		/*
		 * Check the sequence number of the received packet
		 * and report warings if any.
		 */
		if (prev_pkt_num) {
			if ((pkt_recv.pkt_num == prev_pkt_num))
				info("duplicated packet received (curr=%d)",
					pkt_recv.pkt_num);
			else if ((pkt_recv.pkt_num < prev_pkt_num)) /* probable packed duplication */
				info("packet out of order (last=%d curr=%d)",
					prev_pkt_num, pkt_recv.pkt_num);
			else if (pkt_recv.pkt_num != prev_pkt_num + 1) {
				info("%d packets missed (downtime=%luus\n",
				     abs(pkt_recv.pkt_num - prev_pkt_num),
				     elapsed_us);

				prev_pkt_num = pkt_recv.pkt_num;
			} else
				prev_pkt_num = pkt_recv.pkt_num;
		} else
			prev_pkt_num = pkt_recv.pkt_num;

		if (pkt_recv.mode == NETTEST_MODE_ACK) {
			dbg("sending ACK required by the client");
			nsent = send_data(s, comm, &pkt_recv, nrecv);
			err_if_exit(nsent < 0, EXIT_FAILURE,
                                        "cannot send ACK packet: %m");
		}
	}
}

/*
 * Usage
 */

static void usage(void)
{
        fprintf(stderr,
                "usage: %s [-h | --help] [-d | --debug] [-t | --print-time]\n"
                "               [-v | --version]\n"
                "               [-p <port>] [-m addr]\n"
                "               [-i | --use-ethernet <iface>]\n"
                "  defaults are:\n"
                "    - port is %d\n",
                        NAME, NETTEST_UDP_PORT);

        exit(EXIT_FAILURE);
}

/*
 * Main
 */

int main(int argc, char **argv)
{
        int c;
        struct option long_options[] = {
                { "help",               no_argument,            NULL, 'h'},
                { "debug",              no_argument,            NULL, 'd'},
                { "print-time",         no_argument,            NULL, 't'},
                { "version",            no_argument,            NULL, 'v'},
		{ "use-ethernet",       required_argument,      NULL, 'i'},
                { 0, 0, 0, 0    /* END */ }
        };
        int option_index = 0;
	int s;
	struct comm_info_s comm = { .type = NETTEST_INFO_TYPE_UDP };
	unsigned int port = NETTEST_UDP_PORT;
	char *if_name = NULL;
	char *multicast_addr = NULL;

        /*
         * Parse options in command line
         */

        opterr = 0;          /* disbale default error message */
        while (1) {
                option_index = 0; /* getopt_long stores the option index here */

                c = getopt_long(argc, argv, "hdtvp:m:i:",
                                long_options, &option_index);

                /* Detect the end of the options */
                if (c == -1)
                        break;

                switch (c) {
                case 'h':
                        usage();

                case 'd':
                        __debug_level++;
                        break;

                case 't':
                        __add_time++;
                        break;

                case 'v':
                        info("nettestc - ver. %s", NETTEST_VERSION);
                        exit(EXIT_SUCCESS);

                case 'p':
                        port = strtoul(optarg, NULL, 10);
                        err_if_exit(port = 0 || port > 65535,
                                EXIT_FAILURE, "port number must in in [1, 65535]");
                        break;

                case 'i':
                        if_name = optarg;
                        comm.type = NETTEST_INFO_TYPE_ETHERNET;
                        break;

		case 'm':
			multicast_addr = optarg;
			break;

                case ':':
                case '?':
                        err("invalid option %s", argv[optind - 1]);
                        exit(EXIT_FAILURE);

                default:
                        BUG();
		}
	}

        /* Setup communication information */
	switch (comm.type) {
	case NETTEST_INFO_TYPE_UDP:
		comm.proto.udp.port = port;
		comm.proto.udp.multicast_address = multicast_addr;
		break;
	case NETTEST_INFO_TYPE_ETHERNET:
		comm.proto.eth.if_name = if_name;
		break;
	}

        /* Print some useful information and do the job */
	info("running server ver %s", NETTEST_VERSION);
	switch (comm.type) {
	case NETTEST_INFO_TYPE_UDP:
		info("accepting UDP packets on port: %d", comm.proto.udp.port);
		if (comm.proto.udp.multicast_address)
			printf("accepting packets from multicast address %s",
				comm.proto.udp.multicast_address);
		break;
	case NETTEST_INFO_TYPE_ETHERNET:
		info("accepting Ethernet packets on iface: %s",
						comm.proto.eth.if_name);
		break;
	}

	s = open_socket(&comm);
	mainloop(s, &comm);

	return 0;
}
