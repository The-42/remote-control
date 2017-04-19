/*
 * Copyright (C) 2017 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <glib.h>
#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "remote-control.h"

#define POLL_SECOND_SPLITS	8	/* 125 ms */
#define POLL_RATE_US		(1000000 / POLL_SECOND_SPLITS)
#define MAX_MSG_LEN		128

#define DEFAULT_SERVER_IP	"172.21.129.32"
#define DEFAULT_SERVER_PORT	10001
#define DEFAULT_LOCAL_PORT	9999
#define DEFAULT_LOCATION	"000C320C0000"
#define DEFAULT_SYSNAME		"CL003600"
#define DEFAULT_TIMEOUT		10

/*
 * Avaya Medial messages. Inspired by project 1363, padded & salted with bits
 * and pieces supplied by (scarce) documentation, loose chipcards and Wireshark.
 */

struct medial_header {
	uint8_t stx;       /* Start byte */
	uint16_t ref;      /* Ref number */
	char mac[12];      /* MAC Address */
	char ipaddr[8];    /* IP-Addr Key */
	uint16_t type;     /* Package type */
	uint16_t length;   /* Length of bytes follow, stringified hex(*1) */
} __attribute__ ((packed));

/*
 * (*1) Stringified hex is a rather obscure format indicating a packet's
 * payload's length. The number is converted to hex, taken apart, and each half-
 * byte gets the offset of an ASCII '0' applied, 0x30h. This results in things
 * like '30 3D' (ASCII "0=", actually 0x0D or 13), which sort of isn't always
 * readable.
 */

struct medial_powerup {
	struct medial_header header;
	uint8_t uid;       /* Unit ID */
	uint8_t ioport[8]; /* I/O Port CKL unused */
	char acl[8];       /* ACL */
	uint8_t state;     /* Card State */
	char cid[3];       /* Card ID */
	uint8_t mid;       /* Client number*/
	char sname[8];     /* System name */
	char cardno[7];    /* Card number */
	uint8_t etx;       /* end byte */
} __attribute__ ((packed));

struct medial_card_change {
	struct medial_header header;
	uint8_t state;     /* Card state, 30h remove, 31h insert */
	uint8_t type;      /* Card type (Medcom = 60h) */
	char cid[3];       /* Medcom card subtype */
	uint8_t mid;       /* Client number*/
	char sname[8];     /* System name */
	char cardno[7];    /* Card number */
	uint8_t etx;       /* End byte */
} __attribute__ ((packed));

struct medial_response {
	struct medial_header header;
	uint8_t status;    /* Status byte, 30h: ok, 31h: error, 32h: localize */
	uint8_t etx;       /* End byte */
} __attribute__ ((packed));

struct medial_apply {
	struct medial_header header;
	uint8_t  portmode; /* unused */
	uint32_t portmask; /* unused */
	uint32_t portstate;/* unused, except for lower half of 3rd byte(*2) */
	uint32_t time;     /* unused */
	uint8_t etx;       /* End byte */
} __attribute__ ((packed));

/*
 * (*2) This part encodes the balance state, which is either red, yellow or
 * green and has an additional bit indicating whether the account has been
 * closed. Order from MSB (bit 3) to LSB (bit 0): "closed", "green", "yellow",
 * "red". Bits are inversed in their meaning. A status byte of 3Bh thus
 * indicates an open account with green balance, 36h red and closed.
 *
 * NOTE: The balance status currently appears to be broken as it returns Eh
 * (open, red) for any card, regardless of its real state (which is reported
 * correctly via the SOAP interface).
 */

struct medial_parameters {
	char location_id[13];
	char system_name[9];

	char server_ip[16];
	uint16_t server_port;
	uint16_t local_port;

	unsigned int card_number;
	uint8_t card_state;

	unsigned int timeout;
};

struct netcb_data {
	struct net_udp_channel *ch;
	int count;
};

static struct medial_parameters medial;
static struct net_udp *net;
static int channel_ref = 0;

static int init_header(struct medial_header* header, char *mac, char *phone)
{
	char buffer[13];

	if (!header)
		return -EINVAL;

	header->stx = 0x02;
	header->ref = '0' << 8 | '0'; /* ASCII... */
	snprintf(buffer, 13, "%12s", mac);
	strncpy(header->mac, buffer, 12);
	snprintf(buffer, 9, "%8s", phone);
	strncpy(header->ipaddr, buffer, 8);

	return 0;
}

static void send_header_preproc(struct medial_header* header)
{
	if (!header)
		return;

	header->ref = htons(header->ref);
	header->type = htons(header->type);
	header->length = htons(header->length);
}

static void recv_header_postproc(struct medial_header* header)
{
	if (!header)
		return;

	header->ref = ntohs(header->ref);
	header->type = ntohs(header->type);
	header->length = ntohs(header->length);
}

static void recv_apply_postproc(struct medial_apply* appl)
{
	if (!appl)
		return;

	appl->portmask = ntohl(appl->portmask);
	appl->portstate = ntohl(appl->portstate);
	appl->time = ntohl(appl->time);
}

/* set cardno = 0 for no card */
static int init_powerup_message(struct medial_parameters *ctx,
		struct medial_powerup *pwr)
{
	char buffer[9];

	if (!ctx || !pwr)
		return -EINVAL;

	init_header(&pwr->header, ctx->location_id, ctx->system_name);

	pwr->header.type = '5' << 8 | '1';	/* 51h -> PDU 81 */
	pwr->header.length = '2' << 8 | '5';	/* 25h -> 37 + etx */


	pwr->uid = '1';
	memset(pwr->ioport, ' ', 8);
	strncpy(pwr->acl, "AVAYA007", 8);

	pwr->state = ctx->card_state > 0 ? '1' : '0';
	strncpy(pwr->cid, "101", 3);
	pwr->mid = '1';
	snprintf(buffer, 9, "%8s", ctx->system_name);
	strncpy(pwr->sname, buffer, 8);
	snprintf(buffer, 8, "%07d", ctx->card_number);
	strncpy(pwr->cardno, buffer, 7);
	pwr->etx = 0x03;

	return 0;
}

/* set cardno = 0 for no card */
static int init_card_message(struct medial_parameters *ctx,
		struct medial_card_change *card)
{
	char buffer[9];

	if (!ctx || !card)
		return -EINVAL;

	init_header(&card->header, ctx->location_id, ctx->system_name);

	card->header.type = '5' << 8 | '3';	/* 53h -> PDU 83 */
	card->header.length = '1' << 8 | '5';	/* 15h -> 21 + etx */

	card->state = ctx->card_state > 0 ? '1' : '0';
	card->type = 0x60;
	strncpy(card->cid, "101", 3);
	card->mid = '1';
	snprintf(buffer, 9, "%8s", ctx->system_name);
	strncpy(card->sname, buffer, 8);
	snprintf(buffer, 8, "%07d", ctx->card_number);
	strncpy(card->cardno, buffer, 7);
	card->etx = 0x03;

	return 0;
}

static int detect_message_type(uint8_t *data, size_t len)
{
	uint8_t type;

	if (!data || len < 25)
		return -EINVAL;

	type = (data[23] - 0x30) << 4 | (data[24] - 0x30);

	printf("%s: type = 0x%02X (%d)\n", __func__, type, type);

	return type;
}

/* wicked mode: the Medial wants ASCII chars, but returns hex with '0' offset. */
static int print_header(struct medial_header *hdr, char wicked)
{
	uint8_t type;
	uint8_t len;
	int i;

	if (!hdr)
		return -EINVAL;

	printf("\nMedial packet header:\n");
	printf("  stx    = %02X\n", hdr->stx);
	if (wicked) {
		printf("  ref    = %1X%1Xh\n", (hdr->ref >> 8) - 0x30,
			(hdr->ref & 0xFF) - 0x30);
	} else {
		printf("  ref    = '%c%c'\n", (hdr->ref >> 8) & 0xFF,
			hdr->ref & 0xFF);
	}

	printf("  mac    = '");
	for (i = 0; i < sizeof(hdr->mac); i++) {
		if (wicked)
			printf("%1X", hdr->mac[i] - 0x30);
		else
			printf("%c", hdr->mac[i]);
	}
	printf("'\n");

	printf("  ipaddr = '");
	for (i = 0; i < sizeof(hdr->ipaddr); i++)
		printf("%c", hdr->ipaddr[i]);
	printf("'\n");

	type = ((hdr->type >> 8) - 0x30) << 4 | ((hdr->type & 0xFF) - 0x30);
	printf("  type   = %02X %02X => %02Xh => PDU %d\n",
		hdr->type >> 8, hdr->type & 0xFF, type, type);

	len = ((hdr->length >> 8) - 0x30) << 4 | ((hdr->length & 0xFF) - 0x30);
	printf("  length = %02X %02X => %02Xh => %d\n",
		hdr->length >> 8, hdr->length & 0xFF, len, len);

	return 0;
}

static int print_payload_powerup(struct medial_powerup *pwr)
{
	int i;

	if (!pwr)
		return -EINVAL;

	printf("\nMedial 'powerup' packet payload:\n");
	printf("  uid    = '%c'\n", pwr->uid);
	printf("  ioport = '");
	for (i = 0; i < sizeof(pwr->ioport); i++)
		printf("%c", pwr->ioport[i]);
	printf("'\n");

	printf("  acl    = '");
	for (i = 0; i < sizeof(pwr->acl); i++)
		printf("%c", pwr->acl[i]);
	printf("'\n");

	printf("  card   = '%c'\n", pwr->state);

	printf("  cid    = '");
	for (i = 0; i < sizeof(pwr->cid); i++)
		printf("%c", pwr->cid[i]);
	printf("'\n");

	printf("  mid    = '%c'\n", pwr->mid);

	printf("  system = '");
	for (i = 0; i < sizeof(pwr->sname); i++)
		printf("%c", pwr->sname[i]);
	printf("'\n");

	printf("  cardno = '");
	for (i = 0; i < sizeof(pwr->cardno); i++)
		printf("%c", pwr->cardno[i]);
	printf("'\n");

	printf("  etx    = %02X\n", pwr->etx);

	return 0;
}

static int print_payload_card(struct medial_card_change *card)
{
	int i;

	if (!card)
		return -EINVAL;

	printf("\nMedial 'card' packet payload:\n");
	printf("  state  = %c\n", card->state);
	printf("  type   = %02Xh\n", card->type);
	printf("  cid    = '");
	for (i = 0; i < sizeof(card->cid); i++)
		printf("%c", card->cid[i]);
	printf("'\n");

	printf("  mid    = '%c'\n", card->mid);

	printf("  system = '");
	for (i = 0; i < sizeof(card->sname); i++)
		printf("%c", card->sname[i]);
	printf("'\n");

	printf("  cardno = '");
	for (i = 0; i < sizeof(card->cardno); i++)
		printf("%c", card->cardno[i]);
	printf("'\n");

	printf("  etx    = %02X\n", card->etx);

	return 0;
}

static int print_payload_response(struct medial_response *resp)
{
	if (!resp)
		return -EINVAL;

	printf("\nMedial 'response' packet payload:\n");
	printf("  status = %c ", resp->status);
	switch(resp->status) {
	case 0x30:
		printf("(Ok)\n");
		break;

	case 0x31:
		printf("(Error)\n");
		break;

	case 0x32:
		printf("(Location unknown)\n");
		break;

	default:
		printf("(undefined)\n");
		break;
	}

	printf("  etx    = %02X\n", resp->etx);

	return 0;
}

static int print_payload_apply(struct medial_apply *appl)
{
	uint8_t balance;
	int i;

	if (!appl)
		return -EINVAL;

	/* Byte order: it's the 2nd instead of the 3rd byte after ntohl(). */
	balance = (appl->portstate >> 8) & 0x0F;

	printf("\nMedial 'apply' packet payload:\n");
	printf("  pmode  = %02Xh\n", appl->portmode - 0x30);

	printf("  pmask  = ");
	for (i = 0; i < sizeof(appl->portmask); i++)
		printf("%1X", (appl->portmask >> (3 - i) * 8 & 0xFF) - 0x30);
	printf("h\n");

	printf("  pstate = ");
	for (i = 0; i < sizeof(appl->portstate); i++)
		printf("%1X", (appl->portstate >> (3 - i) * 8 & 0xFF) - 0x30);
	printf("h => (%c%c%c%c)\n",
		balance & 0x08 ? '_' : 'C',
		balance & 0x04 ? '_' : 'g',
		balance & 0x02 ? '_' : 'y',
		balance & 0x01 ? '_' : 'r');
	printf("  time   = ");
	for (i = 0; i < sizeof(appl->time); i++)
		printf("%1X", (appl->time >> (3 - i) * 8 & 0xFF) - 0x30);
	printf("h\n");
	printf("  etx    = %02X\n", appl->etx);

	return 0;
}

static void print_raw(uint8_t *data, size_t len)
{
	unsigned int i;

	for (i = 0; i < len; i++) {
		if (!(i % 8))
			printf("\n  %04X:  ", i);
		printf("%02X ", data[i]);
	}
	printf("\n");
}

static void recv_callback(struct net_udp_channel *ch, void *data)
{
	struct netcb_data *ncbd = data;

	g_assert_nonnull(ncbd);
	g_assert_nonnull(ch);

	ncbd->ch = ch;
	ncbd->count++;

	printf("\n  >>> Packet received! %d to fetch. <<<\n\n", ncbd->count);
}

static void cleanup(void)
{
	int ret = net_udp_destroy_channel(net, channel_ref);
	g_assert_cmpint(ret, ==, 0);

	channel_ref = -1;
}

static void sig_handler(int signo)
{
	if (signo == SIGINT) {
		printf("\nSIGINT received, shutting down gracefully...\n");
		cleanup();
	}
}

static ssize_t create_powerup_message(uint8_t **buffer,
		struct medial_parameters *par)
{
	struct medial_powerup msg;
	int ret;

	if (!buffer || !par)
		return -EINVAL;

	*buffer = malloc(sizeof(msg));
	if (!*buffer)
		return -ENOMEM;

	ret = init_powerup_message(par, &msg);
	if (ret < 0) {
		free(*buffer);
		return ret;
	}

	printf("Created powerup packet:\n");
	print_header(&msg.header, 0);
	print_payload_powerup(&msg);
	printf("\n===============\n");

	send_header_preproc(&msg.header);
	memcpy(*buffer, &msg, sizeof(msg));

	return sizeof(msg);
}

static ssize_t create_card_message(uint8_t **buffer,
		struct medial_parameters *par)
{
	struct medial_card_change msg;
	int ret;

	if (!buffer || !par)
		return -EINVAL;

	*buffer = malloc(sizeof(msg));
	if (!*buffer)
		return -ENOMEM;

	ret = init_card_message(par, &msg);
	if (ret < 0) {
		free(*buffer);
		return ret;
	}

	printf("Created card packet:\n");
	print_header(&msg.header, 0);
	print_payload_card(&msg);
	printf("\n===============\n");

	send_header_preproc(&msg.header);
	memcpy(*buffer, &msg, sizeof(msg));

	return sizeof(msg);
}

static void medial_parameters_defaults(struct medial_parameters *par)
{
	if (!par)
		return;

	snprintf(par->location_id, sizeof(par->location_id), "%s",
		DEFAULT_LOCATION);
	snprintf(par->system_name, sizeof(par->system_name), "%s",
		DEFAULT_SYSNAME);
	snprintf(par->server_ip, sizeof(par->server_ip), "%s",
		DEFAULT_SERVER_IP);

	par->server_port = DEFAULT_SERVER_PORT;
	par->local_port = DEFAULT_LOCAL_PORT;

	par->card_number = 0;
	par->card_state = 0;

	par->timeout = DEFAULT_TIMEOUT;
}

static void print_medial_parameters(struct medial_parameters *par)
{
	printf("\nMedial parameter settings:\n");
	printf("  Server IP:    %s\n", par->server_ip);
	printf("  Server port:  %d\n", par->server_port);
	printf("  Local port:   %d\n", par->local_port);
	printf("  System name:  %s\n", par->system_name);
	printf("  Location ID:  %s\n", par->location_id);
	printf("  Card number:  %07d\n", par->card_number);
	printf("  Card state:   %s\n",
		par->card_number > 0 ? "Inserted" : "Removed");
	if (!par->timeout)
		printf("  Timeout:      Never\n");
	else
		printf("  Timeout:      %d s\n", par->timeout);
	printf("\n");
}

static void print_help(void)
{
	printf("Medial test / PoC program\n\n"
		"  -h, --help        print this message\n"
		"  -c, --card        card inserted, 1 or 0\n"
		"  -i, --id          card ID, numeric\n"
		"  -L, --location    location (aka 'room', 'MAC') ID, string\n"
		"  -m, --serverip    Medial server name, string (host or IP)\n"
		"  -p, --serverport  Medial server port, numeric\n"
		"  -P, --localport   local port, numeric\n"
		"  -s, --sysname     System name, string\n"
		"  -t, --timeout     time span for follow-up packets, seconds\n"
		"  -u, --powerup     send 'powerup' instead of 'card changed'\n"
		"  -v, --verbose     print raw packet data\n"
		"\n");
}

int main(int argc, char* argv[])
{
	struct net_udp_channel *net_channel = NULL;
	struct netcb_data netcb_data = { NULL, 0 };
	uint8_t *send_buffer;
	uint8_t *recv_buffer;
	int send_powerup = 0;
	int option_index = 0;
	ssize_t packet_len;
	int timeout = 1;
	int verbose = 0;
	int optsw;
	int type;
	int ret;

	medial_parameters_defaults(&medial);

	while (1) {
		static struct option long_options[] = {
			{"help", no_argument, 0, 'h'},
			{"card", required_argument, 0, 'c'},
			{"id",  required_argument, 0, 'i'},
			{"location",  required_argument, 0, 'L'},
			{"serverip", required_argument, 0, 'm'},
			{"serverport", required_argument, 0, 'p'},
			{"localport", required_argument, 0, 'P'},
			{"sysname", required_argument, 0, 's'},
			{"timeout", required_argument, 0, 't'},
			{"powerup", no_argument, 0, 'u'},
			{"verbose", no_argument, 0, 'v'},
			{0, 0, 0, 0}
		};

		optsw = getopt_long(argc, argv, "c:hi:L:m:p:P:s:t:uv",
			long_options, &option_index);

		if (optsw == -1)
			break;

		switch (optsw) {
		case 'h':
			print_help();
			return 0;

		case 'c':
			errno = 0;
			ret = strtoul(optarg, NULL, 10);
			if (!errno)
				medial.card_state = ret > 0 ? 1 : 0;
			break;

		case 'i':
			errno = 0;
			ret = strtoul(optarg, NULL, 10);
			if (!errno)
				medial.card_number = ret;
			break;

		case 'L':
			snprintf((&medial)->location_id,
				sizeof(medial.location_id), "%s", optarg);
			break;

		case 'm':
			snprintf((&medial)->server_ip,
				sizeof(medial.server_ip), "%s", optarg);
			break;

		case 'p':
			errno = 0;
			ret = strtoul(optarg, NULL, 10);
			if (!errno)
				medial.server_port = ret;
			break;

		case 'P':
			errno = 0;
			ret = strtoul(optarg, NULL, 10);
			if (!errno)
				medial.local_port = ret;
			break;

		case 's':
			snprintf((&medial)->system_name,
				sizeof(medial.system_name), "%s", optarg);
			break;

		case 't':
			errno = 0;
			ret = strtoul(optarg, NULL, 10);
			if (!errno)
				medial.timeout = ret;
			break;

		case 'u':
			send_powerup = 1;
			break;

		case 'v':
			verbose = 1;
			break;

		default:
			break;
		}
	}

	print_medial_parameters(&medial);

	if (signal(SIGINT, sig_handler) == SIG_ERR)
		printf("Warning: Cannot catch SIGINT\n");
	else
		printf("Press CTRL+C to abort.\n");

	ret = net_udp_create(&net);
	g_assert_cmpint(ret, ==, 0);

	channel_ref = net_udp_create_channel(net, medial.local_port,
		medial.server_ip, medial.server_port);
	g_assert_cmpint(channel_ref, >, 0);

	net_channel = net_udp_get_channel_by_ref(net, channel_ref);
	g_assert_nonnull(net_channel);

	ret = net_udp_set_recv_cb(net_channel, recv_callback, &netcb_data);
	g_assert_cmpint(ret, ==, 0);

	if (send_powerup)
		packet_len = create_powerup_message(&send_buffer, &medial);
	else
		packet_len = create_card_message(&send_buffer, &medial);

	ret = net_udp_send(net_channel, send_buffer, packet_len);
	g_assert_cmpint(ret, ==, packet_len);

	if (verbose) {
		printf("Sent:");
		print_raw(send_buffer, ret);
	}

	free(send_buffer);

	if (medial.timeout)
		timeout = medial.timeout * POLL_SECOND_SPLITS;

	while (timeout > 0) {
		if (medial.timeout)
			timeout--;

		usleep(POLL_RATE_US);

		if (channel_ref < 0)
			return EXIT_SUCCESS;

		if (!netcb_data.count)
			continue;

		recv_buffer = malloc(MAX_MSG_LEN);
		g_assert_nonnull(recv_buffer);

		ret = net_udp_recv(net_channel, recv_buffer, MAX_MSG_LEN);
		if (ret < 0) {
			printf("Error receiving message: %d\n", ret);
		} else if (ret > 0) {
			if (verbose) {
				printf("Received:");
				print_raw(recv_buffer, ret);
			}

			if (ret < 27) {
				printf("Packet of size %d cannot possibly "
					"be valid.\n", ret);
			}

			netcb_data.count--;
		}

		if (ret < 27) {
			free(recv_buffer);
			continue;
		}

		type = detect_message_type(recv_buffer, ret);
		recv_header_postproc((struct medial_header *)recv_buffer);

		print_header((struct medial_header *)recv_buffer, 1);

		switch (type) {
		case 0x50:
			print_payload_response((struct medial_response *)recv_buffer);
			break;

		case 0x51:
			print_payload_powerup((struct medial_powerup *)recv_buffer);
			break;

		case 0x53:
			print_payload_card((struct medial_card_change *)recv_buffer);
			break;

		case 0x54:
			recv_apply_postproc((struct medial_apply *)recv_buffer);
			print_payload_apply((struct medial_apply *)recv_buffer);
			break;

		default:
			printf("Unknown message type: %d\n", type);
			break;
		}

		free(recv_buffer);
	}

	printf("Regular listening time (%u s) ends.\n", medial.timeout);

	cleanup();

	return 0;
}

