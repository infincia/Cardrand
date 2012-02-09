/*
 * cardrand.c: A simple wrapper around libopensc for retrieving random data from a smart card HWRNG
 *             and feeding it into the kernel random pool using an ioctl. This makes it available
 *             to any application using /dev/random.
 * Original code for this came from opensc-explorer
 *
 * Copyright (C) 2010 Stephen Oliver <mrsteveman1@gmail.com>
 * Portions copyright (C) 2001  Juha Yrjölä <juha.yrjola@iki.fi>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <opensc/opensc.h>
#include <opensc/asn1.h>
#include <opensc/cardctl.h>

#include <fcntl.h>
#include <errno.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/stat.h>
#include <syslog.h>

#define DIM(v) (sizeof(v)/sizeof((v)[0]))


#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <linux/random.h>

#include "error.h"

#define DEV_RANDOM	"/dev/random"


static const char *app_name = "cardrand";

static int opt_reader = -1, opt_wait = 0, verbose = 0;
static const char *opt_driver = NULL;

static sc_file_t *current_file = NULL;
static sc_path_t current_path;
static sc_context_t *ctx = NULL;
static sc_card_t *card = NULL;

int util_connect_card(sc_context_t *ctx, sc_card_t **cardp,
		 int reader_id, int slot_id, int wait, int verbose)
{
	sc_reader_t *reader;
	sc_card_t *card;
	int r;

	if (wait) {
		sc_reader_t *readers[16];
		int slots[16];
		unsigned int i;
		int j, k, found;
		unsigned int event;

		for (i = k = 0; i < sc_ctx_get_reader_count(ctx); i++) {
			if (reader_id >= 0 && (unsigned int)reader_id != i)
				continue;
			reader = sc_ctx_get_reader(ctx, i);
			for (j = 0; j < reader->slot_count; j++, k++) {
				readers[k] = reader;
				slots[k] = j;
			}
		}

		//printf("Waiting for card to be inserted...\n");
		r = sc_wait_for_event(readers, slots, k,
				SC_EVENT_CARD_INSERTED,
				&found, &event, -1);
		if (r < 0) {
			syslog(LOG_ERR,
				"Error while waiting for card: %s\n",
				sc_strerror(r));
			return 3;
		}

		reader = readers[found];
		slot_id = slots[found];
	} else {
		if (sc_ctx_get_reader_count(ctx) == 0) {
			syslog(LOG_ERR,
				"No smart card readers found.\n");
			return 1;
		}
		if (reader_id < 0) {
			unsigned int i;
			/* Automatically try to skip to a reader with a card if reader not specified */
			for (i = 0; i < sc_ctx_get_reader_count(ctx); i++) {
				reader = sc_ctx_get_reader(ctx, i);
				if (sc_detect_card_presence(reader, 0) & SC_SLOT_CARD_PRESENT) {
					reader_id = i;
					syslog(LOG_NOTICE, "Using reader with a card: %s\n", reader->name);
					goto autofound;
				}
			}
			reader_id = 0;
		}
autofound:
		if ((unsigned int)reader_id >= sc_ctx_get_reader_count(ctx)) {
			syslog(LOG_ERR,
				"Illegal reader number. "
				"Only %d reader(s) configured.\n",
				sc_ctx_get_reader_count(ctx));
			return 1;
		}

		reader = sc_ctx_get_reader(ctx, reader_id);
		slot_id = 0;
		if (sc_detect_card_presence(reader, 0) <= 0) {
			syslog(LOG_ERR, "Card not present.\n");
			return 3;
		}
	}

	if (verbose)
		printf("Connecting to card in reader %s...\n", reader->name);
	if ((r = sc_connect_card(reader, slot_id, &card)) < 0) {
		syslog(LOG_ERR,
			"Failed to connect to card: %s\n",
			sc_strerror(r));
		return 1;
	}

	if (verbose)
		printf("Using card driver %s.\n", card->driver->name);

	if ((r = sc_lock(card)) < 0) {
		syslog(LOG_ERR,
			"Failed to lock card: %s\n",
			sc_strerror(r));
		sc_disconnect_card(card, 0);
		return 1;
	}

	*cardp = card;
	return 0;
}

static void die(int ret)
{
	if (card) {
		sc_unlock(card);
		sc_disconnect_card(card, 0);
	}
	if (ctx)
		sc_release_context(ctx);
	exit(ret);
}


static void do_random()
{
	unsigned char buffer[128];
	int r, count = 128;
	int n = 128;
	int n_bits = 1024;

        struct rand_pool_info *output;
	int fd = open(DEV_RANDOM, O_WRONLY);
	if (fd == -1)
		syslog(LOG_ERR,"Failed to open %s", DEV_RANDOM);

	while (1) {

		r = sc_get_challenge(card, buffer, count);
		if (r < 0) {
			syslog(LOG_ERR,"Failed to get random bytes: %s\n", sc_strerror(r));
		}
		//printf("Card returned: %s\n",buffer);

        	output = (struct rand_pool_info *)malloc(sizeof(struct rand_pool_info) + n);
        	if (!output) {
                	syslog(LOG_ERR,"malloc failure in kernel_rng_add_entropy_no_bitcount_increase(%d)", n);
		}
		output -> entropy_count = n_bits;
		output -> buf_size      = n;
		memcpy(&(output -> buf[0]), buffer, n);

		if (ioctl(fd, RNDADDENTROPY, output) == -1) {
			syslog(LOG_ERR,"ioctl(RNDADDENTROPY) failed!");
		}
		free(output);
		sleep(1);
	}
	close(fd);
}

int main(int argc, char * const argv[])
{
	daemon(0,1);
	//setsid();
	//int i=fork();
	//if (i<0) exit(1);
	//if (i>0) exit(0);
	int r, c, long_optind = 0, err = 0;
	char *line;
	int cargc;
	char *cargv[20];
	sc_context_param_t ctx_param;

	memset(&ctx_param, 0, sizeof(ctx_param));
	ctx_param.ver      = 0;
	ctx_param.app_name = app_name;

	r = sc_context_create(&ctx, &ctx_param);
	if (r) {
		syslog(LOG_ERR, "Failed to establish context: %s\n", sc_strerror(r));
		return 1;
	}

	err = util_connect_card(ctx, &card, opt_reader, 0, opt_wait, 0);
	if (err)
		goto end;

	{
		int lcycle = SC_CARDCTRL_LIFECYCLE_ADMIN;
		r = sc_card_ctl(card, SC_CARDCTL_LIFECYCLE_SET, &lcycle);
		if (r && r != SC_ERROR_NOT_SUPPORTED)
			syslog(LOG_ERR, "unable to change lifecycle: %s\n",
				sc_strerror(r));
	}
	do_random();
	goto end;
end:
	die(err);
	
	return 0; /* not reached */
}
