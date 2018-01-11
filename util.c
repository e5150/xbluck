/*
 * Copyright © 2017 Lars Lindqvist <lars.lindqvist at yandex.ru>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <err.h>

#include "xbluck.h"

extern struct options_t conf;
extern int state;

void
debugprint(int level, const char *fmt, ...) {
	if (conf.debug >= level) {
		va_list ap;
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
	}
}

int
log_state() {
	char timestr[20];
	time_t t;
	struct tm *tmp;
	int ret = 0;
	FILE *fp = NULL;

	if (conf.logfile && conf.logfile[0]) {
		if (!(fp = fopen(conf.logfile, "a"))) {
			warn("fopen %s", conf.logfile);
		}
	} else if (!conf.verbose) {
		return 0;
	}

	if (!fp) {
		fp = stderr;
	}

	t = time(NULL);
	if ((tmp = localtime(&t)) && strftime(timestr, 20, "%Y-%m-%d %H:%M:%S", tmp)) {
		ret += fprintf(fp, "%s ", timestr);
	} else {
		ret += fprintf(fp, "%ld", t);
	}

	switch (state) {
	case STATE_FAILED:
		ret += fprintf(fp, "failed attempt\n");
		break;
	case STATE_LOCKED:
		ret += fprintf(fp, "locked\n");
		break;
	case STATE_UNLOCK:
		ret += fprintf(fp, "unlock\n");
		break;
	}

	if (fp != stderr)
		fclose(fp);

	return ret;
}

long
estrtol(const char *s, int base) {
	char *end;
	long n;

	errno = 0;
	n = strtol(s, &end, base);
	if (errno != 0) {
		err(1, "strtol %s %d", s, base);
	} else if (*end != '\0') {
		errx(1, "strtol %s: Not a base %d integer", s, base);
	} else {
		return n;
	}
	exit(1);
}
