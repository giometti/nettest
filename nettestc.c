#include <getopt.h>
#include "nettest.h"

int nettest_debug_level;
int nettest_add_time;

static unsigned int period_ms = 1000;
static unsigned int packets_num = 0;
static unsigned int send_ack = 0;
static unsigned int udp_port = NETTEST_UDP_PORT;
static unsigned int packet_size = NETTEST_PACKET_SIZE;

/*
 * Local functions
 */

static int open_socket(void)
{
	int s;
	int on;
	u_char ttl;
	int ret;

	s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	err_if_exit(s < 0, EXIT_FAILURE, "unable to open socket: %m");

	/* Set socket to allow broadcast */
	on = 1;
	ret = setsockopt(s, SOL_SOCKET, SO_BROADCAST,
				(void *) &on, sizeof(on));
	err_if_exit(ret < 0, EXIT_FAILURE, "cannot set broadcast permission: %m");

	/* Set multicast TTL for multicast packets */
	ttl = 5;
	ret = setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
	err_if_exit(ret < 0, EXIT_FAILURE, "cannot set multicast TTL: %m");

	return s;
}

static void send_data(int s, char *dst_addr)
{
	int done;
	struct data_packet pkt_sent, pkt_recv;
	int data_size = sizeof(unsigned int) + packet_size;
	struct sockaddr_in srv_addr, cli_addr;
	socklen_t n;
	ssize_t nsent, nrecv;
	struct timeval t1, t2, t_after;
	long delta_s, delta_u;
	unsigned int elapsed_us, period_us;
	unsigned long long rtt_us_avg;
	unsigned int cnt;
	int i, ret;

	/*
	 * The command on the first packet of the stream should be the
	 * NETTEST_CMD_START command.
	 */
	pkt_sent.command = NETTEST_CMD_START;

	/* enable ACK mode if required */
	pkt_sent.mode = send_ack ? NETTEST_MODE_ACK : NETTEST_MODE_NONE;

	/* Initialize the rest of transmitted structure */
	pkt_sent.pkt_num = 0;
	pkt_sent.period_ms = period_ms;
	for (i = 0; i < packet_size; i++)
		pkt_sent.filler[i] = i;

	/* Compute the size of the packet to transmit.
	 * The packet structure is declared with a static payload of
	 * NETTEST_FILLER_SIZE bytes.
	 */
	data_size = sizeof(pkt_sent) - NETTEST_FILLER_SIZE + packet_size;
	period_us = period_ms * 1000;

	srv_addr.sin_family = AF_INET;
	srv_addr.sin_port = htons(udp_port);
	srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	/* srv_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST); */

	ret = inet_aton(dst_addr, &srv_addr.sin_addr);
	err_if_exit(ret < 0, EXIT_FAILURE, "cannot convert IP address");

	rtt_us_avg = 0;
	cnt = 0;

	done = 0;
	while (!done) {
		if (send_ack)
			gettimeofday(&t1, NULL);

		nsent = sendto(s, &pkt_sent, data_size, 0,
			    (struct sockaddr *) &srv_addr, sizeof(srv_addr));
		err_if_exit(nsent < 0, EXIT_FAILURE, "cannot send packet: %m");
		gettimeofday(&t_after, NULL);
		dbg("transmitted %ld bytes", nsent);

		/* Switch com CMD_NONE after sending the first packet */
		if (pkt_sent.pkt_num == 0)
			pkt_sent.command = NETTEST_CMD_NONE;
		if (pkt_sent.command == NETTEST_CMD_STOP)
			done = 1;
		pkt_sent.pkt_num++;

		/*
		 * if we have choosen to send a predefined number of packets
		 * send a CMD_STOP for signaling the last packet.
		 */
		if (packets_num && pkt_sent.pkt_num > packets_num)
			pkt_sent.command = NETTEST_CMD_STOP;

		/*
		 * If we have enabled packets acknowledge we shoud wait for a
		 * response from the partner
		 */
		if (send_ack) {
			bzero(&cli_addr, sizeof(cli_addr));
			n = sizeof(cli_addr);

			nrecv = recvfrom(s, &pkt_recv, sizeof(pkt_recv), 0,
			      (struct sockaddr *) &cli_addr, &n);
			err_if_exit(nrecv < 0, EXIT_FAILURE,
					"cannot receive ACK  packet: %m");
			gettimeofday(&t2, NULL);

			delta_s = t2.tv_sec - t1.tv_sec;
			delta_u = t2.tv_usec - t1.tv_usec;

			cnt++;

			elapsed_us = (delta_s) * 1000000 + delta_u;
			rtt_us_avg += elapsed_us;
			dbg("got ACK (RTT=%uus)", elapsed_us);
		} else {
			if (period_us)
				usleep(period_us);
		}
	}
	info("transmitted %u packets of %d bytes", pkt_sent.pkt_num, data_size);
	if (send_ack)
		info("average RTT: %lluus", rtt_us_avg / cnt);
}

/*
 * Usage
 */

static void usage(void)
{
        fprintf(stderr,
                "usage: %s [-h | --help] [-d | --debug] [-t | --print-time]\n"
                "               [-v | --version]\n"
                "               [-p <port>] [-s <size>] [-f <period>] [-n <packets>] [-a]  <addr>\n"
		"  defaults are:\n"
		"    - port is %d\n"
		"    - size is %d bytes for payload\n"
		"    - period is %dms\n"
		"    - packets is %d packets to be sent\n",
			NAME, udp_port, packet_size, period_ms, packets_num);

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
	char *addr = NULL;
	int min_packet_size = sizeof(struct data_packet) -
				NETTEST_FILLER_SIZE + 2,
	    max_packet_size = sizeof(struct data_packet) + 2;
	int s;

        /*
         * Parse options in command line
         */

        opterr = 0;          /* disbale default error message */
        while (1) {
                option_index = 0; /* getopt_long stores the option index here */

                c = getopt_long(argc, argv, "hdtvp:s:f:n:a",
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

		case 'a':
			send_ack = 1;
			break;

		case 's':
			packet_size = strtoul(optarg, NULL, 10);
			err_if_exit(packet_size < min_packet_size, EXIT_FAILURE,
				    "packet size too small. Min allowed size "
				    "is %d bytes", min_packet_size);
			err_if_exit(packet_size > max_packet_size, EXIT_FAILURE,
				    "packet size too large. Max allowed size "
				    "is %d bytes", max_packet_size);
			break;

		case 'f':
			period_ms = strtoul(optarg, NULL, 10);
			break;

		case 'n':
			packets_num = strtoul(optarg, NULL, 10);
			break;

		case 'p':
			udp_port = strtoul(optarg, NULL, 10);
			err_if_exit(udp_port = 0 || udp_port > 65535,
				EXIT_FAILURE, "port number must in in [1, 65535]");
			break;

		case ':':
		case '?':
			err("invalid option %s", argv[optind - 1]);
			exit(EXIT_FAILURE);

		default:
			BUG();
		}
	}
	if (argc - optind < 1)
		usage();
	addr = argv[optind];

	info("running UDP client ver %s.", NETTEST_VERSION);
	info("connecting with UDP server at %s:%d", addr, udp_port);
	if (period_ms)
		info("sending %d bytes packets every %dms", packet_size,
								period_ms);
	else
		info("sending %d bytes packets at wire speed", packet_size);
	if (packets_num)
		info("total packets number to transmit is %u", packets_num);
	if (send_ack)
		info("ACK reception is enabled");

	s = open_socket();
	send_data(s, addr);

	return 0;
}
