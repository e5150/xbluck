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

#include <ctype.h>
#include <errno.h>
#include <err.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>

#include "xbluck.h"

extern struct options_t conf;
extern struct auth_t auth;

int state = STATE_LOCKED;

int
main(int argc, char **argv) {

	parse_options(argc, argv);

	if (conf.hash && conf.hash[0] != '\0' && crypt("", conf.hash)) {
		DEBUG(3, "HASH=%s", conf.hash);
		auth.hash = conf.hash;
	} else {
#ifndef __OpenBSD__
		if (init_auth() < 0) {
			errx(1, "Unable to initialize auth");
		}
		if (!crypt("", auth.hash)) {
			err(1, "crypt %s", auth.hash);
		}
#endif
	}
	drop_priv();
	reset_input();
	xcb_init();
	mainloop();
	xcb_close();
	auth_destroy();

	return 0;
}
