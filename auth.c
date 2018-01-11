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

#include <sys/types.h>
#include <grp.h>
#include <pwd.h>
#include <err.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#ifdef __OpenBSD__
#include <login_cap.h>
#include <bsd_auth.h>
#else
#include <shadow.h>
#endif

#include "xbluck.h"

struct auth_t auth;

#ifndef explicit_bzero
void explicit_bzero(void *buf, size_t len) {
	bzero(buf, len);
}
#endif

void
reset_input() {
	auth.cursor = 0;
	explicit_bzero(auth.input, sizeof(auth.input));
}

static struct passwd *
egetpwuid(uid_t uid) {
	struct passwd *pw;
	errno = 0;
	if ((!(pw = getpwuid(uid)))) {
		errx(1, "getpwuid %d: %s", uid,
		     errno ? strerror(errno) : "No such user");
	}
	return pw;
}

bool
password_is_valid() {
	const char *hash;
	bool valid = false;

	if (auth.hash) {
		if (!(hash = crypt(auth.input, auth.hash))) {
			warn("crypt");
		} else {
			valid = strcmp(hash, auth.hash) == 0;
		}
	} else {
#ifdef __OpenBSD__
		struct passwd *pw = egetpwuid(getuid());
		valid = auth_userokay(pw->pw_name, NULL, NULL, auth.input);
#endif
	}

	return valid;
}

int
init_auth() {
	auth.hash = NULL;
	
	struct passwd *pw = egetpwuid(getuid());
	if (!strcmp(pw->pw_passwd, "*")) {
		warnx("User has no password");
		return -1;
	} else if (!strcmp(pw->pw_passwd, "x")) {
		struct spwd *sp;
		errno = 0;
		if (!(sp = getspnam(pw->pw_name))) {
			warnx("getspnam %s: %s", pw->pw_name,
			      errno ? strerror(errno) : "No shadow entry");
			return -1;
		}
		auth.hash = sp->sp_pwdp;
	} else {
		auth.hash = pw->pw_passwd;
	}
	return 0;
}

void
drop_priv() {
	if (!geteuid() && getuid()) {
		struct passwd *pw = egetpwuid(getuid());
		if (setgid(pw->pw_gid) < 0) {
			err(1, "setgid %d", pw->pw_gid);
		}
		if (setuid(pw->pw_uid) < 0) {
			err(1, "setuid %d", pw->pw_uid);
		}
	}
}

void
auth_destroy() {
	explicit_bzero(&auth, sizeof(auth));
}
