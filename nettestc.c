#include <getopt.h>
#include "nettest.h"

int nettest_debug_level;
int nettest_add_time;

/*
 * Local functions
 */

static int open_socket(struct comm_info_s *comm)
{
	int s;
	int on;
	u_char ttl;
	int ret;

	switch (comm->type) {
	case NETTEST_INFO_TYPE_UDP:
		dbg("selected communication protocol is UDP");

		s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		err_if_exit(s < 0, EXIT_FAILURE, "unable to open socket: %m");

		/* Set socket to allow broadcast */
		on = 1;
		ret = setsockopt(s, SOL_SOCKET, SO_BROADCAST,
					(void *) &on, sizeof(on));
		err_if_exit(ret < 0, EXIT_FAILURE,
					"cannot set broadcast permission: %m");

		/* Set multicast TTL for multicast packets */
		ttl = 5;
		ret = setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
		err_if_exit(ret < 0, EXIT_FAILURE,
					"cannot set multicast TTL: %m");

		/* Complete UDP struct sockaddr_in data */
		comm->proto.udp.raw_address.sin_family = AF_INET;
		comm->proto.udp.raw_address.sin_port =
					htons(comm->proto.udp.port);
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
	switch (comm->type) {
	case NETTEST_INFO_TYPE_UDP:
		return sendto(s, pkt, len, 0,
				(struct sockaddr *) &comm->proto.udp.raw_address,
				sizeof(comm->proto.udp.raw_address));

        default:
                err("unsupported communication protocol!");
                exit(EXIT_FAILURE);
        }
}

static ssize_t recv_data(int s, struct comm_info_s *comm,
				struct data_packet_s *pkt, size_t len)
{
	struct sockaddr_in addr;
	socklen_t addr_len;

        switch (comm->type) {
        case NETTEST_INFO_TYPE_UDP:
		bzero(&addr, sizeof(addr));
		addr_len = sizeof(addr);
		return recvfrom(s, pkt, len, 0,
				(struct sockaddr *) &addr, &addr_len);

        default:
                err("unsupported communication protocol!");
                exit(EXIT_FAILURE);
        }
}

static void mainloop(int s, struct comm_info_s *comm)
{
	int done;
	struct data_packet_s pkt_sent, pkt_recv;
	int data_size = sizeof(unsigned int) + comm->packet_size;
	ssize_t nsent, nrecv;
	struct timeval t1, t2, t_after;
	long delta_s, delta_u;
	unsigned int elapsed_us, period_us;
	unsigned long long rtt_us_avg;
	unsigned int cnt;
	int i;

	/*
	 * The command on the first packet of the stream should be the
	 * NETTEST_CMD_START command.
	 */
	pkt_sent.command = NETTEST_CMD_START;

	/* enable ACK mode if required */
	pkt_sent.mode = comm->use_ack ? NETTEST_MODE_ACK : NETTEST_MODE_NONE;

	/* Initialize the rest of transmitted structure */
	pkt_sent.pkt_num = 0;
	pkt_sent.period_ms = comm->period_ms;
	for (i = 0; i < comm->packet_size; i++)
		pkt_sent.filler[i] = i;

	/* Compute the size of the packet to transmit.
	 * The packet structure is declared with a static payload of
	 * NETTEST_FILLER_SIZE bytes.
	 */
	data_size = sizeof(pkt_sent) - NETTEST_FILLER_SIZE + comm->packet_size;

	period_us = comm->period_ms * 1000;
	rtt_us_avg = 0;
	cnt = 0;
	done = 0;
	while (!done) {
		if (comm->use_ack)
			gettimeofday(&t1, NULL);

		nsent = send_data(s, comm, &pkt_sent, data_size);
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
		if (comm->packets_num && pkt_sent.pkt_num > comm->packets_num)
			pkt_sent.command = NETTEST_CMD_STOP;

		/*
		 * If we have enabled packets acknowledge we shoud wait for a
		 * response from the partner
		 */
		if (comm->use_ack) {
			nrecv = recv_data(s, comm, &pkt_recv, sizeof(pkt_recv));
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
	if (comm->use_ack)
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
		"    - period is %dms",
			NAME, NETTEST_UDP_PORT, NETTEST_PACKET_SIZE,
				NETTEST_PERIOD_MS);

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
	int min_packet_size = sizeof(struct data_packet_s) -
				NETTEST_FILLER_SIZE + 2,
	    max_packet_size = sizeof(struct data_packet_s) + 2;
	int s;
	struct comm_info_s comm = { .type = NETTEST_INFO_TYPE_UDP };
	unsigned int port = NETTEST_UDP_PORT;
	size_t packet_size = NETTEST_PACKET_SIZE;
	unsigned int period_ms = NETTEST_PERIOD_MS;
	bool use_ack = 0;
	static unsigned int packets_num = 0;
	char *str;

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
			use_ack = 1;
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
			port = strtoul(optarg, NULL, 10);
			err_if_exit(port = 0 || port > 65535,
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

	/* Setup communication information */
	switch (comm.type) {
	case NETTEST_INFO_TYPE_UDP:
		comm.proto.udp.port = port;
		break;
	}
	nettest_set_address(&comm, argv[optind]);
	comm.packet_size = packet_size;
	comm.period_ms = period_ms;
	comm.packets_num = packets_num;
	comm.use_ack = use_ack;

	/* Print some useful information and do the job */
	info("running client ver %s.", NETTEST_VERSION);
	info("connecting with %s server at %s",
				nettest_get_proto(&comm),
				str = nettest_get_address(&comm));
	free(str);
	if (comm.period_ms)
		info("sending %ld bytes packets every %dms",
				comm.packet_size, comm.period_ms);
	else
		info("sending %ld bytes packets at wire speed",
				comm.packet_size);
	if (comm.packets_num)
		info("total packets number to transmit is %u",
				comm.packets_num);
	if (comm.use_ack)
		info("ACK reception is enabled");

	s = open_socket(&comm);
	mainloop(s, &comm);

	return 0;
}
