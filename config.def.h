static const char *default_colors[STATE_NUM] = {
	[STATE_LOCKED] = "#101010",
	[STATE_INPUT]  = "#005577",
	[STATE_ERASE]  = "#C08030",
	[STATE_FAILED] = "#FF2010",
	[STATE_UNLOCK] = "#407040",
};

static const int default_timeout = 250;
static const int default_border = 5;
static const char default_logfile[] = "";
static const char default_hash[] = "";

#define FILTER(name, param) { filter_##name, filter_check_##name, { param } }
static const struct filter_t default_filters[] = {
	FILTER(pixelate, 2),
	FILTER(noise, 0x10),
};
