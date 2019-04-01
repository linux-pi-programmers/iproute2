/* Copyright (C) 2013 Cisco Systems, Inc, 2013.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Author: Vijay Subramanian <vijaynsu@cisco.com>
 * Author: Mythili Prabhu <mysuryan@cisco.com>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <math.h>

#include "utils.h"
#include "tc_util.h"

static void explain(void)
{
	fprintf(stderr, "Usage: ... pi [ limit PACKETS ][ target LENGTH PACKETS]\n");
	fprintf(stderr, "              [ tupdate TIME us][ alpha ALPHA ]");
	fprintf(stderr, "[beta BETA ][bytemode | nobytemode][ecn | noecn ]\n");
}

#define ALPHA_MAX 32
#define BETA_MAX 32

static int pi_parse_opt(struct qdisc_util *qu, int argc, char **argv,
			 struct nlmsghdr *n, const char *dev)
{
	unsigned int limit   = 0;
	unsigned int target  = 0;
	unsigned int tupdate = 0;
	unsigned int alpha   = 0;
	unsigned int beta    = 0;
	double tmp;
	int ecn = -1;
	int bytemode = -1;
	struct rtattr *tail;

	while (argc > 0) {
		if (strcmp(*argv, "limit") == 0) {
			NEXT_ARG();
			if (get_unsigned(&limit, *argv, 0)) {
				fprintf(stderr, "Illegal \"limit\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "target") == 0) {
			NEXT_ARG();
			if (get_unsigned(&target, *argv, 0)) { // change to get_unsigned
				fprintf(stderr, "Illegal \"target\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "tupdate") == 0) {
			NEXT_ARG();
			if (get_time(&tupdate, *argv)) {
				fprintf(stderr, "Illegal \"tupdate\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "alpha") == 0) {
			NEXT_ARG();
			if (sscanf(*argv, "%lg", &tmp) != 1 ||
			    (tmp > ALPHA_MAX)) {
				fprintf(stderr, "Illegal \"alpha\"\n");
				return -1;
			}
			alpha = (unsigned int) tmp * 100000000;
		} else if (strcmp(*argv, "beta") == 0) {
			NEXT_ARG();
			if (sscanf(*argv, "%lg", &tmp) != 1 ||
			    (tmp > BETA_MAX)) {
				fprintf(stderr, "Illegal \"beta\"\n");
				return -1;
			}
			beta = (unsigned int) tmp * 100000000;
		} else if (strcmp(*argv, "ecn") == 0) {
			ecn = 1;
		} else if (strcmp(*argv, "noecn") == 0) {
			ecn = 0;
		} else if (strcmp(*argv, "bytemode") == 0) {
			bytemode = 1;
		} else if (strcmp(*argv, "nobytemode") == 0) {
			bytemode = 0;
		} else if (strcmp(*argv, "help") == 0) {
			explain();
			return -1;
		} else {
			fprintf(stderr, "What is \"%s\"?\n", *argv);
			explain();
			return -1;
		}
		argc--;
		argv++;
	}

	tail = addattr_nest(n, 1024, TCA_OPTIONS);
	if (limit)
		addattr_l(n, 1024, TCA_PI_LIMIT, &limit, sizeof(limit));
	if (tupdate)
		addattr_l(n, 1024, TCA_PI_TUPDATE, &tupdate, sizeof(tupdate));
	if (target)
		addattr_l(n, 1024, TCA_PI_TARGET, &target, sizeof(target));
	if (alpha) // Need to add scaling here 
		addattr_l(n, 1024, TCA_PI_ALPHA, &alpha, sizeof(alpha));
	if (beta) // Need to add scaling here 
		addattr_l(n, 1024, TCA_PI_BETA, &beta, sizeof(beta));
	if (ecn != -1)
		addattr_l(n, 1024, TCA_PI_ECN, &ecn, sizeof(ecn));
	if (bytemode != -1)
		addattr_l(n, 1024, TCA_PI_BYTEMODE, &bytemode,
			  sizeof(bytemode));

	addattr_nest_end(n, tail);
	return 0;
}

static int pi_print_opt(struct qdisc_util *qu, FILE *f, struct rtattr *opt)
{
	struct rtattr *tb[TCA_PI_MAX + 1];
	unsigned int limit;
	unsigned int tupdate;
	unsigned int target;
	unsigned int alpha;
	unsigned int beta;
	unsigned int ecn;
	unsigned int bytemode;

	SPRINT_BUF(b1);

	if (opt == NULL)
		return 0;

	parse_rtattr_nested(tb, TCA_PI_MAX, opt);

	if (tb[TCA_PI_LIMIT] &&
	    RTA_PAYLOAD(tb[TCA_PI_LIMIT]) >= sizeof(__u32)) {
		limit = rta_getattr_u32(tb[TCA_PI_LIMIT]);
		fprintf(f, "limit %up ", limit);
	}
	if (tb[TCA_PI_TARGET] &&
	    RTA_PAYLOAD(tb[TCA_PI_TARGET]) >= sizeof(__u32)) {
		target = rta_getattr_u32(tb[TCA_PI_TARGET]);
		fprintf(f, "target %s ", sprint_time(target, b1));
	}
	if (tb[TCA_PI_TUPDATE] &&
	    RTA_PAYLOAD(tb[TCA_PI_TUPDATE]) >= sizeof(__u32)) {
		tupdate = rta_getattr_u32(tb[TCA_PI_TUPDATE]);
		fprintf(f, "tupdate %s ", sprint_time(tupdate, b1));
	}
	if (tb[TCA_PI_ALPHA] &&
	    RTA_PAYLOAD(tb[TCA_PI_ALPHA]) >= sizeof(__u32)) {
		alpha = rta_getattr_u32(tb[TCA_PI_ALPHA]);
		fprintf(f, "alpha %u ", alpha);
	}
	if (tb[TCA_PI_BETA] &&
	    RTA_PAYLOAD(tb[TCA_PI_BETA]) >= sizeof(__u32)) {
		beta = rta_getattr_u32(tb[TCA_PI_BETA]);
		fprintf(f, "beta %u ", beta);
	}

	if (tb[TCA_PI_ECN] && RTA_PAYLOAD(tb[TCA_PI_ECN]) >= sizeof(__u32)) {
		ecn = rta_getattr_u32(tb[TCA_PI_ECN]);
		if (ecn)
			fprintf(f, "ecn ");
	}

	if (tb[TCA_PI_BYTEMODE] &&
	    RTA_PAYLOAD(tb[TCA_PI_BYTEMODE]) >= sizeof(__u32)) {
		bytemode = rta_getattr_u32(tb[TCA_PI_BYTEMODE]);
		if (bytemode)
			fprintf(f, "bytemode ");
	}

	return 0;
}

static int pi_print_xstats(struct qdisc_util *qu, FILE *f,
			    struct rtattr *xstats)
{
	struct tc_pi_xstats *st;

	if (xstats == NULL)
		return 0;

	if (RTA_PAYLOAD(xstats) < sizeof(*st))
		return -1;

	st = RTA_DATA(xstats);
	/*prob is returned as a fracion of maximum integer value */
	fprintf(f, "prob %f\n",
		(double)st->prob / (double)0xffffffff);
	fprintf(f, "pkts_in %u overlimit %u dropped %u maxq %u ecn_mark %u\n",
		st->packets_in, st->overlimit, st->dropped, st->maxq,
		st->ecn_mark);
	return 0;

}

struct qdisc_util pi_qdisc_util = {
	.id = "pi",
	.parse_qopt	= pi_parse_opt,
	.print_qopt	= pi_print_opt,
	.print_xstats	= pi_print_xstats,
};
