#pragma once
#include <stdint.h>
#include <stdbool.h>

struct auth_t {
	const char *hash;
	char input[128];
	size_t cursor;
};

int init_auth();
void auth_destroy();
bool password_is_valid();
void reset_input();
void drop_priv();

enum {
	STATE_LOCKED,
	STATE_INPUT,
	STATE_ERASE,
	STATE_FAILED,
	STATE_UNLOCK,
	STATE_NUM
};

void debugprint(int, const char *, ...);
#define DEBUG(lev, fmt, ...) do { \
	debugprint(lev, "%s:%d:%s:" fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
} while (0)

#define LENGTH(x) (sizeof(x) / sizeof(*x))

int log_state();
long estrtol(const char *, int);
void drop_privs(const char *user, const char *group);
const char *get_hash();

struct fpus_t {
	uint32_t u1, u2;
};

union fparam_t {
	int32_t i;
	uint32_t u;
	struct fpus_t us;
	double d;
};

struct filter_t {
	void (*function)(uint32_t*, int, int, union fparam_t);
	void (*checker)(union fparam_t);
	union fparam_t param;
};

#define FILTERFUNC(name) \
void filter_##name(uint32_t *img, int w, int h, union fparam_t param)

#define FILTERCHK(name) \
void filter_check_##name(union fparam_t param)

#define FILTERPROT(name) \
FILTERFUNC(name); \
FILTERCHK(name)

FILTERPROT(flip);
FILTERPROT(flop);
FILTERPROT(null);
FILTERPROT(invert);
FILTERPROT(colourise);
FILTERPROT(shift);
FILTERPROT(gaussian);
FILTERPROT(pixelate);
FILTERPROT(noise);
FILTERPROT(tile);
FILTERPROT(greyscale);
FILTERPROT(edge);
void apply_filters(uint32_t *img, int w, int h, struct filter_t *filters, int n);

struct options_t {
	int timeout;
	int border;
	struct filter_t *filters;
	size_t nfilter;

	int debug;
	int verbose;
	bool invert;
	const char *logfile;
	const char *hash;
	const char *colors[STATE_NUM];
	
};

void parse_options(int argc, char **argv);

void xcb_init();
void xcb_close();
void mainloop();
