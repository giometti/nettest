#include <getopt.h>
#include "nettest.h"

int nettest_debug_level;
int nettest_add_time;

static unsigned int udp_port = NETTEST_UDP_PORT;
static unsigned int packet_size = NETTEST_PACKET_SIZE;

static int prompt_n;
static char prompt_symbol[] = { '|', '/', '-', '\\' };

static int open_socket(void)
{
	int s;
	struct sockaddr_in addr;
	int ret;

	s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        err_if_exit(s < 0, EXIT_FAILURE, "unable to open socket: %m");

	addr.sin_family = AF_INET;
	addr.sin_port = htons(udp_port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	ret = bind(s, (struct sockaddr *) &addr, sizeof(addr));
	err_if_exit(ret < 0, EXIT_FAILURE, "cannot bind socket: %m");

	return s;
}

static void receive_data(int s, char *maddr, int filler_size)
{
	struct sockaddr_in cli_addr;
	socklen_t n;
	int receive = 1;
	unsigned int prev_pkt_num;
	int data_size;
	static struct data_packet packet;
	struct ip_mreq mc_request;	/* multicast request structure */
	struct timeval t1, t2, t3;
	long delta_s, delta_u, delta_point;
	unsigned long elapsed_us;
	unsigned long long elapsed_avg_us;
	ssize_t nrecv, nsent;

	if (maddr) {
		/* construct a IGMP join request structure */
		mc_request.imr_multiaddr.s_addr = inet_addr(maddr);
		mc_request.imr_interface.s_addr = htonl(INADDR_ANY);

		/* send an ADD MEMBERSHIP message via setsockopt */
		if ((setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP,
				(void *)&mc_request, sizeof(mc_request))) < 0) {
			perror("setsockopt() failed");
			exit(1);
		}
	}

	prev_pkt_num = 0;
	data_size = sizeof(packet);

	elapsed_avg_us = 0;
	t1.tv_sec = t2.tv_sec = t3.tv_sec = 0;
	t1.tv_usec = t2.tv_usec = t3.tv_usec = 0;
	delta_s = delta_u = delta_point = 0;

	bzero(&cli_addr, sizeof(cli_addr));
	n = sizeof(cli_addr);

	while (receive) {
		nrecv = recvfrom(s, &packet, data_size, 0,
			      (struct sockaddr *) &cli_addr, &n);
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

		if (packet.command == NETTEST_CMD_START) {
			info("new transmission detected, resetting counters");

			if (packet.period_ms)
				info("frequency announced is 1 packet "
						"every %dms", packet.period_ms);
			else
				info("frequency announced is at wire speed");

			info("client address is %s:%u",
				       inet_ntoa(cli_addr.sin_addr),
				       ntohs(cli_addr.sin_port));

			prev_pkt_num = 0;
			elapsed_avg_us = 0;
			t1.tv_usec = t3.tv_usec = 0;
			t1.tv_sec = t3.tv_sec = 0;
		} else if (packet.command == NETTEST_CMD_STOP)
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
		if (nettest_debug_level == 0 &&
		     (t2.tv_sec - t3.tv_sec) > 1) {
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
		dbg("recv pkt=%u/%u from=%s size=%ld ipt=%luus",
			     packet.pkt_num, prev_pkt_num,
			     inet_ntoa(cli_addr.sin_addr), nrecv,
			     elapsed_us);

		/*
		 * Check the sequence number of the received packet
		 * and report warings if any.
		 */
		if (prev_pkt_num) {
			if ((packet.pkt_num == prev_pkt_num))
				info("duplicated packet received (curr=%d)",
					packet.pkt_num);
			else if ((packet.pkt_num < prev_pkt_num)) /* probable packed duplication */
				info("packet out of order (last=%d curr=%d)",
					prev_pkt_num, packet.pkt_num);
			else if (packet.pkt_num != prev_pkt_num + 1) {
				info("%d packets missed (downtime=%luus\n",
				     abs(packet.pkt_num - prev_pkt_num),
				     elapsed_us);

				prev_pkt_num = packet.pkt_num;
			} else
				prev_pkt_num = packet.pkt_num;
		} else
			prev_pkt_num = packet.pkt_num;

		if (packet.mode == NETTEST_MODE_ACK) {
			dbg("sending ACK required by the client");
			nsent = (sendto(s, &packet, nrecv, 0,
				      (struct sockaddr *)&cli_addr,
				      sizeof(cli_addr)));
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
                "               [-p <port>] [-s <size>] [-m addr]\n"
                "  defaults are:\n"
                "    - port is %d\n"
                "    - size is %d bytes for payload\n",
                        NAME, udp_port, packet_size);

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
                { 0, 0, 0, 0    /* END */ }
        };
        int option_index = 0;
	int s;
	char *maddr = NULL;

        /*
         * Parse options in command line
         */

        opterr = 0;          /* disbale default error message */
        while (1) {
                option_index = 0; /* getopt_long stores the option index here */

                c = getopt_long(argc, argv, "hdtvp:s:m:",
                                long_options, &option_index);

                /* Detect the end of the options */
                if (c == -1)
                        break;

                switch (c) {
                case 'h':
                        usage();

                case 'd':
                        nettest_debug_level++;
                        break;

                case 't':
                        nettest_add_time++;
                        break;

                case 'v':
                        info("nettestc - ver. %s", NETTEST_VERSION);
                        exit(EXIT_SUCCESS);

                case 'p':
                        udp_port = strtoul(optarg, NULL, 10);
                        err_if_exit(udp_port = 0 || udp_port > 65535,
                                EXIT_FAILURE, "port number must in in [1, 65535]");
                        break;

		case 's':
			packet_size = strtoul(optarg, NULL, 10);
			break;

		case 'm':
			maddr = optarg;
			break;

                case ':':
                case '?':
                        err("invalid option %s", argv[optind - 1]);
                        exit(EXIT_FAILURE);

                default:
                        BUG();
		}
	}

	info("running UDP server ver %s", NETTEST_VERSION);
	info("accepting packets on port: %d", udp_port);
	if (maddr)
		printf("accepting packets from multicast address %s", maddr);
	info("waiting for packets of %d bytes", packet_size);

	s = open_socket();
	receive_data(s, maddr, packet_size);

	return 0;
}
