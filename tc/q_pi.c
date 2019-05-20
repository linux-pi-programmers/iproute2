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
	fprintf(stderr, "Usage: ... pi [ limit PACKETS ][ qref LENGTH PACKETS]\n");
	fprintf(stderr, "              [ w TIME us][ a A ]");
	fprintf(stderr, "[b B ][bytemode | nobytemode][ecn | noecn ]\n");
}

#define A_MAX 10
#define B_MAX 1

static int pi_parse_opt(struct qdisc_util *qu, int argc, char **argv,
			 struct nlmsghdr *n, const char *dev)
{
	unsigned int limit   = 0;
	unsigned int qref  = 0;
	unsigned int w = 0;
	unsigned long int a = 0;
	unsigned long int b = 0;
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
		} else if (strcmp(*argv, "qref") == 0) {
			NEXT_ARG();
			if (get_unsigned(&qref, *argv, 0)) { // change to get_unsigned
				fprintf(stderr, "Illegal \"qref\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "w") == 0) {
			NEXT_ARG();
			if (get_time(&w, *argv)) {
				fprintf(stderr, "Illegal \"w\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "a") == 0) {
			NEXT_ARG();
			if (get_u64(&a, *argv, 0) ||
			    (a > A_MAX)) {
				fprintf(stderr, "Illegal \"a\"\n");
				return -1;
			}
		} else if (strcmp(*argv, "b") == 0) {
			NEXT_ARG();
			if (get_u64(&b, *argv, 0) ||
			    (tmp > B_MAX)) {
				fprintf(stderr, "Illegal \"b\"\n");
				return -1;
			}
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
	if (w)
		addattr_l(n, 1024, TCA_PI_W, &w, sizeof(w));
	if (qref)
		addattr_l(n, 1024, TCA_PI_QREF, &qref, sizeof(qref));
	if (a) // Need to add scaling here 
		addattr_l(n, 1024, TCA_PI_A, &a, sizeof(a));
	if (b) // Need to add scaling here 
		addattr_l(n, 1024, TCA_PI_B, &b, sizeof(b));
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
	unsigned int w;
	unsigned int qref;
	unsigned int a;
	unsigned int b;
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
	if (tb[TCA_PI_QREF] &&
	    RTA_PAYLOAD(tb[TCA_PI_QREF]) >= sizeof(__u32)) {
		qref = rta_getattr_u32(tb[TCA_PI_QREF]);
		fprintf(f, "qref %s ", sprint_time(qref, b1));
	}
	if (tb[TCA_PI_W] &&
	    RTA_PAYLOAD(tb[TCA_PI_W]) >= sizeof(__u32)) {
		w = rta_getattr_u32(tb[TCA_PI_W]);
		fprintf(f, "w %s ", sprint_time(w, b1));
	}
	if (tb[TCA_PI_A] &&
	    RTA_PAYLOAD(tb[TCA_PI_A]) >= sizeof(__u64)) {
		a = rta_getattr_u64(tb[TCA_PI_A]);
		fprintf(f, "a %u ", a);
	}
	if (tb[TCA_PI_B] &&
	    RTA_PAYLOAD(tb[TCA_PI_B]) >= sizeof(__u64)) {
		b = rta_getattr_u64(tb[TCA_PI_B]);
		fprintf(f, "b %u ", b);
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
	fprintf(f, "prob %f qlen %d\n",
		(double)st->prob / (double)0xffffffff, st->qlen);
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
