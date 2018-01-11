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

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <err.h>

#include "xbluck.h"
#include "config.h"

struct options_t conf = { 0 };


static void
add_filter(void (*fn)(uint32_t *img, int w, int h, union fparam_t param),
           void (*chk)(union fparam_t param),
           union fparam_t param) {
	conf.filters = realloc(conf.filters, (conf.nfilter + 1) * sizeof(struct filter_t));
	conf.filters[conf.nfilter].function = fn;
	conf.filters[conf.nfilter].checker = chk;
	conf.filters[conf.nfilter].param = param;
	++conf.nfilter;
}

#define ADD_FILTER(name, param) \
	add_filter(filter_##name, filter_check_##name, (union fparam_t) { param })

enum {
	OPT_COL_LOCKED,
	OPT_COL_INPUT,
	OPT_COL_ERASE,
	OPT_COL_FAILED,
	OPT_COL_UNLOCK,
	OPT_GENHASH,
	OPT_CONF_HASH,
	OPT_CONF_QUIET       = 'q',
	OPT_CONF_TIMEOUT     = 'T',
	OPT_CONF_BORDER      = 'B',
	OPT_CONF_LOGFILE     = 'L',
	OPT_CONF_DEBUG       = 'D',
	OPT_SHOW_USAGE       = 'h',
	OPT_FILTER_GAUSSIAN  = 'g',
	OPT_FILTER_PIXELATE  = 'p',
	OPT_FILTER_NOISE     = 'n',
	OPT_FILTER_COLOURISE = 'c',
	OPT_FILTER_TILE      = 't',
	OPT_FILTER_INVERT    = 'i',
	OPT_FILTER_NULL      = 'S',
	OPT_FILTER_SHIFT     = 'Z',
	OPT_FILTER_GREY      = 'G',
	OPT_FILTER_FLIP      = 'F',
	OPT_FILTER_FLOP      = 'f',
	OPT_FILTER_EDGE      = 'E',
};
static const char optstr[] = "B:L:D::g:p:n:c:t:iSZ:GFfhE";

static void
usage(void) {
	printf("usage: %s [-%c timeout] [-%c logfile] [-%c borderwidth] [--hash hash] [colours] [filters]\n",
	       program_invocation_name, OPT_CONF_TIMEOUT, OPT_CONF_LOGFILE, OPT_CONF_BORDER);
	printf("Options:\n");
	printf("\t--timeout <msec> : Timeout after successfully unlocking\n");
	printf("\t                   default: %d\n", default_timeout);
	printf("\t--logfile <path> : Logfile for unlock attempts.\n");
	printf("\t                   (stderr in none, unless -q)\n");
	printf("\t                   default: %s\n", default_logfile);
	printf("\t--hash <hash>    : Password hash to use instead of\n");
	printf("\t                   user's default. See crypt(3).\n");
	printf("\t--genhash[=salt] : Prompts for password and prints its hash.\n");
	printf("\t--border <width> : Width of border. default: %d\n", default_border);
	printf("\t-D               : Enable debugging, may be given multiple times\n");
	printf("\t                   At debug level 1, any three bytes is taken\n");
	printf("\t                   to be a valid password.\n");
	printf("Colours: Border colour for the different runtime states.\n");
	printf("\t--colour-locked  (default: %s)\n", default_colors[STATE_LOCKED]);
	printf("\t--colour-input   (default: %s)\n", default_colors[STATE_INPUT]);
	printf("\t--colour-erase   (default: %s)\n", default_colors[STATE_ERASE]);
	printf("\t--colour-failed  (default: %s)\n", default_colors[STATE_FAILED]);
	printf("\t--colour-unlock  (default: %s)\n", default_colors[STATE_UNLOCK]);
	printf("Filters: If any filter(s) is given on the command line, it\n");
	printf("will be used instead of the filter(s) given at compile time.\n");
	printf("Multiple filters can be chained and/or repeated in any order,\n");
	printf("and will be applied as given on the command line.\n");
	printf("\t-%c|--blur <r>           : Gaußian blur by <r> radius.\n", OPT_FILTER_GAUSSIAN);
	printf("\t-%c|--pixelate <s>       : Pixelation of <s>x<s>\n", OPT_FILTER_PIXELATE);
	printf("\t-%c|--colourise <colour> : Colourise image with <colour>=#AARRGGBB\n", OPT_FILTER_COLOURISE);
	printf("\t-%c|--noise <±level>     : Add random noise of [<-level>, <+level>]\n", OPT_FILTER_NOISE);
	printf("\t                        : to each colour channel.\n");
	printf("\t-%c|--tile <x,y>         : Tile miniature screenshots\n", OPT_FILTER_TILE);
	printf("\t-%c|--invert             : Invert all colours\n", OPT_FILTER_INVERT);
	printf("\t-%c|--null               : No-op filter\n", OPT_FILTER_NULL);
	printf("\t-%c|--flip               : Flip image\n", OPT_FILTER_FLIP);
	printf("\t-%c|--flop               : Flop image\n", OPT_FILTER_FLOP);
	printf("\t-%c|--edge               : Edge-detection\n", OPT_FILTER_EDGE);
	printf("\t-%c|--shift <n>          : Shift every line by ±<n> pixels\n", OPT_FILTER_SHIFT);
	printf("\t-%c|--grey               : Convert to grey-scale\n", OPT_FILTER_GREY);
	exit(1);
}

static struct option long_opts[] = {
	{ "blur",  1, 0, OPT_FILTER_GAUSSIAN },
	{ "gauss", 1, 0, OPT_FILTER_GAUSSIAN },
	{ "pixelate", 1, 0, OPT_FILTER_PIXELATE },
	{ "colourise", 1, 0, OPT_FILTER_COLOURISE },
	{ "noise", 1, 0, OPT_FILTER_NOISE },
	{ "tile", 1, 0, OPT_FILTER_TILE },
	{ "invert", 0, 0, OPT_FILTER_INVERT },
	{ "null", 0, 0, OPT_FILTER_NULL },
	{ "flip", 0, 0, OPT_FILTER_FLIP },
	{ "flop", 0, 0, OPT_FILTER_FLOP },
	{ "edge", 0, 0, OPT_FILTER_EDGE },
	{ "shift", 1, 0, OPT_FILTER_SHIFT },
	{ "grey", 1, 0, OPT_FILTER_SHIFT },
	{ "border", 1, 0, OPT_CONF_BORDER },
	{ "timeout", 1, 0, OPT_CONF_TIMEOUT },
	{ "logfile", 1, 0, OPT_CONF_LOGFILE },
	{ "hash", 1, 0, OPT_CONF_HASH },
	{ "debug", 2, 0, OPT_CONF_DEBUG },
	{ "quiet", 0, 0, OPT_CONF_QUIET },
	{ "genhash", 2, 0, OPT_GENHASH },
	{ "colour-locked", 1, 0, OPT_COL_LOCKED },
	{ "colour-input",  1, 0, OPT_COL_INPUT },
	{ "colour-erase",  1, 0, OPT_COL_ERASE },
	{ "colour-failed", 1, 0, OPT_COL_FAILED },
	{ "colour-unlock", 1, 0, OPT_COL_UNLOCK },
	{ "help", 0, 0, OPT_SHOW_USAGE },
	{ 0, 0, 0, 0 },
};

static void
mkfpus(const char *str, struct fpus_t *us) {
	char *end;
	errno = 0;

	us->u1 = strtol(str, &end, 0);
	if (errno)
		err(1, "strtol %s 0", str);
	if (*end != ',')
		errx(1, "Invalid argument: %s is not comma separated", str);
	us->u2 = estrtol(end + 1, 0);
}
	
void
parse_options(int argc, char **argv) {
	struct fpus_t us;
	uint32_t u;
	char *c;
	size_t i;

	conf.hash = default_hash;
	conf.border = default_border;
	conf.timeout = default_timeout;
	conf.filters = NULL;
	conf.nfilter = 0;

	for (i = 0; i < STATE_NUM; ++i)
		conf.colors[i] = default_colors[i];

	conf.logfile = getenv("XBLUCK_LOGFILE");
	if (!conf.logfile)
		conf.logfile = default_logfile;

	int opt, optidx;
	while (-1 != (opt = getopt_long(argc, argv, optstr, long_opts, &optidx))) {
		switch (opt) {
		case OPT_GENHASH:
			if (!(c = getpass("Enter password: ")))
				err(1, "getpass");
			if (!(c = crypt(c, optarg ? optarg : "00")))
				err(1, "crypt");
			puts(c);
			exit(0);
		case OPT_COL_LOCKED:
			conf.colors[STATE_LOCKED] = optarg;
			break;
		case OPT_COL_INPUT:
			conf.colors[STATE_INPUT] = optarg;
			break;
		case OPT_COL_ERASE:
			conf.colors[STATE_ERASE] = optarg;
			break;
		case OPT_COL_FAILED:
			conf.colors[STATE_FAILED] = optarg;
			break;
		case OPT_COL_UNLOCK:
			conf.colors[STATE_UNLOCK] = optarg;
			break;

		case OPT_CONF_QUIET:
			--conf.verbose;
			break;
		case OPT_CONF_DEBUG:
			++conf.debug;
			if (optarg)
				conf.debug = strlen(optarg);
			break;
		case OPT_CONF_HASH:
			conf.hash = optarg;
			break;
		case OPT_CONF_TIMEOUT:
			conf.timeout = estrtol(optarg, 0);
			break;
		case OPT_CONF_BORDER:
			conf.border = estrtol(optarg, 0);
			break;
		case OPT_CONF_LOGFILE:
			conf.logfile = optarg;
			break;

		case OPT_FILTER_GAUSSIAN:
			u = estrtol(optarg, 0);
			ADD_FILTER(gaussian, u);
			break;
		case OPT_FILTER_PIXELATE:
			u = estrtol(optarg, 0);
			ADD_FILTER(pixelate, u);
			break;
		case OPT_FILTER_NOISE:
			u = estrtol(optarg, 0);
			ADD_FILTER(noise, u);
			break;
		case OPT_FILTER_SHIFT:
			u = estrtol(optarg, 0);
			ADD_FILTER(shift, u);
			break;
		case OPT_FILTER_COLOURISE:
			u = optarg[0] == '#' ?  estrtol(optarg + 1, 16) : estrtol(optarg, 0);
			ADD_FILTER(colourise, u);
			break;
		case OPT_FILTER_TILE:
			mkfpus(optarg, &us);
			ADD_FILTER(tile, .us = us);
			break;
		case OPT_FILTER_EDGE:
			ADD_FILTER(edge, 0);
			break;
		case OPT_FILTER_GREY:
			ADD_FILTER(greyscale, 0);
			break;
		case OPT_FILTER_INVERT:
			ADD_FILTER(invert, 0);
			break;
		case OPT_FILTER_NULL:
			ADD_FILTER(null, 0);
			break;
		case OPT_FILTER_FLIP:
			ADD_FILTER(flip, 0);
			break;
		case OPT_FILTER_FLOP:
			ADD_FILTER(flop, 0);
			break;
		case OPT_SHOW_USAGE:
		default:
			usage();
		}
	}
	if (optind < argc)
		usage();

	if (!conf.filters) {
		conf.filters = (struct filter_t*)default_filters;
		conf.nfilter = LENGTH(default_filters);
	}

	for (i = 0; i < conf.nfilter; ++i) {
		conf.filters[i].checker(conf.filters[i].param);
	}
}
