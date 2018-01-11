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

#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon.h>
#include <xcb/randr.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <string.h>
#include <ctype.h>
#include <poll.h>

#include "xbluck.h"

extern struct options_t conf;
extern int state;
extern struct auth_t auth;

struct screen_t {
	xcb_screen_t *screen;
	xcb_window_t win;
	xcb_gcontext_t gc;
	xcb_colormap_t cmap;
	xcb_pixmap_t pix;
	struct {
		int w, h;
		uint32_t len;
		uint32_t *data;
	} img;

	struct rect_t *mons;
	int nmon;

	uint32_t colors[STATE_NUM];
	uint32_t border[STATE_NUM];
};

struct rect_t {
	int x, y, w, h;
};

static int rrbase = -1;

static struct screen_t **screens = NULL;
static int nscreens = 0;
static xcb_connection_t *conn;
static xcb_key_symbols_t *ksyms;

static void
check_xcb_cookie(xcb_void_cookie_t cookie, const char *fmt, ...) {
	xcb_generic_error_t *error;
	if ((error = xcb_request_check(conn, cookie))) {
		fprintf(stderr, "%s: fatal xcb error: ", program_invocation_name);
		xcb_disconnect(conn);
		va_list ap;
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
		fprintf(stderr, ": %d\n", error->error_code);
		exit(1);
	}
}

static void
set_fg(xcb_gcontext_t gc, uint32_t val) {
	xcb_void_cookie_t cookie;
	cookie = xcb_change_gc(conn, gc, XCB_GC_FOREGROUND, &val);
	check_xcb_cookie(cookie, "could not change gc foreground to 0x%08x", val);
}

static void
put_image(struct screen_t *screen) {
	xcb_void_cookie_t cookie;
	cookie = xcb_put_image(conn, XCB_IMAGE_FORMAT_Z_PIXMAP, screen->win,
	                       screen->gc, screen->img.w, screen->img.h, 0, 0, 0,
	                       screen->screen->root_depth,
	                       screen->img.len, (uint8_t*)screen->img.data);
	if (conf.debug > 2) {
		xcb_rectangle_t r = { 50, 50, 50, 50 };
		int i;
		for (i = 0; i < STATE_NUM; ++i, r.x += 100) {
			r.y = 50;
			set_fg(screen->gc, screen->colors[i]);
			xcb_poly_fill_rectangle(conn, screen->win, screen->gc, 1, &r);
			r.y = 150;

			set_fg(screen->gc, screen->border[i]);
			xcb_poly_fill_rectangle(conn, screen->win, screen->gc, 1, &r);
		}
	}
	check_xcb_cookie(cookie, "could not put image");
}

static void
set_border(struct screen_t *screen) {
	int i;
	int b = conf.border;
	for (i = 0; i < screen->nmon; ++i) {
		int x = screen->mons[i].x;
		int y = screen->mons[i].y;
		int w = screen->mons[i].w;
		int h = screen->mons[i].h;
		DEBUG(2, "screen=%p color=%s x=%d y=%d w=%d h=%d",
		      (void*)screen, conf.colors[state], x, y, w, h);

		xcb_rectangle_t rs[] = {
			{ x+1, y+1, b-2, h-2 },
			{ x+w-b+1, y+1, b-2, h-2},
			{ x+b-1, y+1, w-2*b+2, b-2 },
			{ x+b-1, y+h-b+1, w-2*b+2, b-2 },
		};
		b -= 1; w -= 1; h -= 1;
		xcb_rectangle_t ro[] = {
			{ x+0, y+0, w, h },
			{ x+b, y+b, w-2*b, h-2*b},
		};

		set_fg(screen->gc, screen->colors[state]);
		xcb_poly_fill_rectangle(conn, screen->win, screen->gc, 4, rs);
		set_fg(screen->gc, screen->border[state]);
		xcb_poly_rectangle(conn, screen->win, screen->gc, 2, ro);
	}
}

static void
set_borders() {
	int i;
	for (i = 0; i < nscreens; i++) {
		set_border(screens[i]);
	}
}

static void
set_monitors(struct screen_t *screen) {
	free(screen->mons);
	screen->nmon = 0;

	if (rrbase >= 0) {
		xcb_generic_error_t *error;
		xcb_randr_get_monitors_cookie_t cookie;
		xcb_randr_get_monitors_reply_t *reply;

		cookie = xcb_randr_get_monitors(conn, screen->screen->root, true);
		reply = xcb_randr_get_monitors_reply(conn, cookie, &error);
		if (error || !reply) {
			errx(1, "unable to get randr monitors: %d", error->error_code);
		}

		xcb_randr_monitor_info_iterator_t iter;
		screen->nmon = xcb_randr_get_monitors_monitors_length(reply);
		screen->mons = malloc(screen->nmon * sizeof(struct rect_t));
		if (!screen->mons) {
			err(1, "malloc");
		}

		DEBUG(1, "screen=%p root=%012x nmon=%d", (void*)screen, screen->screen->root, screen->nmon);

		struct rect_t *mon;
		for (iter = xcb_randr_get_monitors_monitors_iterator(reply), mon = screen->mons;
		     iter.rem;
		     xcb_randr_monitor_info_next(&iter), ++mon) {
			const xcb_randr_monitor_info_t *mi = iter.data;
			mon->x = mi->x;
			mon->y = mi->y;
			mon->w = mi->width;
			mon->h = mi->height;
			DEBUG(1, "screen=%p root=%012x x=%d y=%d w=%d h=%d prim=%d nout=%d",
			      (void*)screen, screen->screen->root,
			      mon->x, mon->y, mon->w, mon->h,
			      mi->primary, mi->nOutput);
		}
		free(reply);
	} else {
		screen->mons = malloc(1 * sizeof(struct rect_t));
		screen->nmon = 1;
		screen->mons[0].x = 0;
		screen->mons[0].y = 0;
		screen->mons[0].w = screen->screen->width_in_pixels;
		screen->mons[0].h = screen->screen->height_in_pixels;
	}
}

static struct screen_t *
find_screen_by_window(xcb_window_t win, bool check_root) {
	int i;
	for (i = 0; i < nscreens; ++i) {
		if ((check_root && win == screens[i]->screen->root)
		|| (!check_root && win == screens[i]->win)) {
			return screens[i];
		}
	}
	return NULL;
}

static void
handle_map_notify(xcb_map_notify_event_t *ev) {
	struct screen_t *screen = find_screen_by_window(ev->window, false);
	DEBUG(2, "XCB_MAP_NOTIFY:win=%d (screen=%p)", ev->window, (void*)screen);
	if (screen) {
		put_image(screen);
		set_border(screen);
		xcb_flush(conn);
	}
	if (conf.debug > 5) {
		exit(0);
	}
}

static void
handle_expose(xcb_expose_event_t *ev) {
	struct screen_t *screen = find_screen_by_window(ev->window, false);
	DEBUG(2, "XCB_EXPOSE:x=%d y=%d w=%d h=%d (screen=%p)",
	      ev->x, ev->y, ev->width, ev->height, (void*)screen);
	if (screen) {
		put_image(screen);
		set_border(screen);
		xcb_flush(conn);
	}
}

static void
handle_key_release(xcb_key_release_event_t *ev) {
	DEBUG(2, "XCB_KEY_RELEASE:keycode=0x%04x state=0x%04x", ev->detail, ev->state);
	if (state != STATE_UNLOCK) {
		state = STATE_LOCKED;
	}
}

static void
handle_configure_notify(xcb_configure_notify_event_t *ev) {
	struct screen_t *screen = find_screen_by_window(ev->window, true);
	DEBUG(2, "XCB_CONFIGURE_NOTIFY:x=%d y=%d w=%d h=%d (screen=%p)",
	      ev->x, ev->y, ev->width, ev->height, (void*)screen);
	if (screen) {
		uint32_t mask;
		uint32_t vals[2];

		mask = XCB_CONFIG_WINDOW_WIDTH
		     | XCB_CONFIG_WINDOW_HEIGHT;
		vals[0] = ev->width;
		vals[1] = ev->height;
		xcb_configure_window(conn, screen->win, mask, vals);
		set_border(screen);
		xcb_flush(conn);
	}
}


static void
handle_key_press(xcb_key_press_event_t *ev) {
	xcb_keysym_t ksym;
	char buf[32];
	int len;

	ksym = xcb_key_press_lookup_keysym(ksyms, ev, ev->state);
	DEBUG(2, "XCB_KEY_PRESS:keycode=0x%04x state=0x%04x (ksym=0x%04x)", ev->detail, ev->state, ksym);

	memset(buf, '\0', sizeof(buf));

	if (xcb_is_private_keypad_key(ksym)
	 || xcb_is_function_key(ksym)
	 || xcb_is_misc_function_key(ksym)
	 || xcb_is_pf_key(ksym)) {
		warnx("ignoring %d", ksym);
		return;
	}

	switch (ksym) {
	case XKB_KEY_Return:
	case XKB_KEY_KP_Enter:
		state = password_is_valid() ? STATE_UNLOCK : STATE_FAILED;
		if (conf.debug && auth.cursor == 3)
			state = STATE_UNLOCK;
		reset_input();
		log_state();
		break;
	case XKB_KEY_Escape:
		if (state != STATE_FAILED) {
			state = STATE_FAILED;
			reset_input();
		}
		break;
	case XKB_KEY_BackSpace:
		state = STATE_ERASE;
		while (auth.cursor > 0) {
			char *c = auth.input + --auth.cursor;
			if (((*c) & 0xC0) != 0x80)
				break;
		}
		auth.input[auth.cursor] = '\0';
		if (!auth.cursor) {
			state = STATE_FAILED;
		}
		break;
	default:
		/* TODO: handle compose and other magic that XKeysymToString took care of */
		/* len includes terminating null byte */
		len = xkb_keysym_to_utf8(ksym, buf, sizeof(buf));
		if (len < 0) {
			if (conf.verbose) warnx("xkb_keysym_to_utf8: buf to small: %d", ksym);
		} else if (len == 0) {
			if (conf.verbose) warnx("xkb_keysym_to_utf8: no such unicode point: %d", ksym);
		} else if (len == 1) {
			if (conf.verbose) warnx("xkb_keysym_to_utf8: null: %d", ksym);
		} else {
			if (auth.cursor + len < sizeof(auth.input)) {
				memcpy(auth.input + auth.cursor, buf, len);
				auth.cursor += len - 1;
				state = STATE_INPUT;
			}
		}
	}
}

static void
handle_screen_change(xcb_randr_screen_change_notify_event_t *ev) {
	struct screen_t *screen = find_screen_by_window(ev->root, true);
	DEBUG(2, "XCB_RANDR_SCREEN_CHANGE_NOTIFY:w=%d h=%d (screen=%p)",
	      ev->width, ev->height, (void*)screen);
	if (screen) {
		set_monitors(screen);
		xcb_flush(conn);
	}
}

void
mainloop() {
	struct pollfd pfd;

	pfd.fd = xcb_get_file_descriptor(conn);
	pfd.events = POLLIN;

	log_state();

	while (state != STATE_UNLOCK && poll(&pfd, 1, -1) == 1) {
		xcb_generic_event_t *ev;
		int old = state;

		if (pfd.revents & ~POLLIN) {
			errx(1, "poll %d: revents=%04x", pfd.fd, pfd.revents);
		}
		
		xcb_flush(conn);
		while ((ev = xcb_poll_for_event(conn))) {
			int type = ev->response_type & 0x7f;
			if (type == XCB_MAP_NOTIFY) {
				handle_map_notify((xcb_map_notify_event_t*)ev);
			} else if (type == XCB_EXPOSE) {
				handle_expose((xcb_expose_event_t*)ev);
			} else if (type == XCB_KEY_RELEASE) {
				handle_key_release((xcb_key_release_event_t*)ev);
			} else if (type == XCB_KEY_PRESS) {
				handle_key_press((xcb_key_press_event_t*)ev);
			} else if (type == XCB_CONFIGURE_NOTIFY) {
				handle_configure_notify((xcb_configure_notify_event_t*)ev);
			} else if (rrbase >= 0 && type == rrbase + XCB_RANDR_SCREEN_CHANGE_NOTIFY) {
				handle_screen_change((xcb_randr_screen_change_notify_event_t*)ev);
			} else {
				DEBUG(2, "Unhandled event=%d", type);
			}
			free(ev);
		}
		if (state != old) {
			set_borders();
			xcb_flush(conn);
		}
	}
	usleep(conf.timeout * 1000);
}

static int
grab_inputs(xcb_screen_t *screen, int attempts) {
	bool pg = false;
	bool kg = false;

	while (--attempts) {
		if (!pg) {
			xcb_grab_pointer_cookie_t cookie;
			xcb_grab_pointer_reply_t *reply;

			cookie = xcb_grab_pointer(conn,
			                          false,
			                          screen->root,
			                          XCB_NONE,
			                          XCB_GRAB_MODE_ASYNC,
			                          XCB_GRAB_MODE_ASYNC,
			                          XCB_NONE,
			                          XCB_CURSOR_NONE,
			                          XCB_CURRENT_TIME);
			reply = xcb_grab_pointer_reply(conn, cookie, NULL);
			if (reply) {
				if (reply->status == XCB_GRAB_STATUS_SUCCESS) {
					pg = true;
				}
				free(reply);
			}
		}
		if (!kg) {
			xcb_grab_keyboard_cookie_t cookie;
			xcb_grab_keyboard_reply_t *reply;

			cookie = xcb_grab_keyboard(conn,
			                           true,
			                           screen->root,
			                           XCB_CURRENT_TIME,
			                           XCB_GRAB_MODE_ASYNC,
			                           XCB_GRAB_MODE_ASYNC);
			reply = xcb_grab_keyboard_reply(conn, cookie, NULL);
			if (reply) {
				if (reply->status == XCB_GRAB_STATUS_SUCCESS) {
					kg = true;
				}
				free(reply);
			}
			
		}
		if (pg && kg) {
			DEBUG(2, "attemps left=%d", attempts);
			return 0;
		}
		usleep(1000);
	}
	return -1;
}

static xcb_alloc_named_color_reply_t *
get_named_color_reply(xcb_colormap_t cmap, const char *name) {
	xcb_generic_error_t *error = NULL;
	xcb_alloc_named_color_cookie_t cookie;
	xcb_alloc_named_color_reply_t *reply;

	cookie = xcb_alloc_named_color(conn, cmap, strlen(name), name);
	reply = xcb_alloc_named_color_reply(conn, cookie, &error);

	if (error || !reply) {
		errx(1, "failed to alloc color %s: %d", name, error->error_code);
	}
	return reply;
}

static uint32_t
get_color(xcb_colormap_t cmap, uint16_t r, uint16_t g, uint16_t b) {
	xcb_generic_error_t *error = NULL;
	xcb_alloc_color_cookie_t cookie;
	xcb_alloc_color_reply_t *reply;
	uint32_t ret;

	cookie = xcb_alloc_color(conn, cmap, htobe16(r), htobe16(g), htobe16(b));
	reply = xcb_alloc_color_reply(conn, cookie, &error);
	if (error || !reply) {
		errx(1, "failed to alloc color %d %d %d: %d", r, g, b, error->error_code);
	}
	ret = reply->pixel;
	free(reply);
	return ret;
}

static void
init_colors(struct screen_t *screen) {
	xcb_colormap_t cmap = screen->screen->default_colormap;
	int i;

	for (i = 0; i < STATE_NUM; ++i) {
		const char *name = conf.colors[i];
		uint16_t r, g, b;

		if (name[0] == '#') {
			char rs[3] = { name[1], name[2], '\0' };
			char gs[3] = { name[3], name[4], '\0' };
			char bs[3] = { name[5], name[6], '\0' };
			r = estrtol(rs, 16);
			g = estrtol(gs, 16);
			b = estrtol(bs, 16);

			screen->colors[i] = get_color(cmap, r, g, b);
		} else {
			xcb_alloc_named_color_reply_t *reply;
			reply = get_named_color_reply(cmap, name);

			screen->colors[i] = reply->pixel;
			r = reply->exact_red;
			g = reply->exact_green;
			b = reply->exact_blue;
			free(reply);
		}
		r += 0x10 * (r < 0x80 ? 1 : -1);
		g += 0x10 * (g < 0x80 ? 1 : -1);
		b += 0x10 * (b < 0x80 ? 1 : -1);
		screen->border[i] = get_color(cmap, r, g, b);

	}
}

static void
create_image(struct screen_t *screen) {
	uint32_t *tmp, *src;
	size_t len;
	int w, h;

	screen->img.w = w = screen->screen->width_in_pixels;
	screen->img.h = h = screen->screen->height_in_pixels;

	xcb_get_image_cookie_t cookie;
	xcb_get_image_reply_t *imgrep;
	xcb_generic_error_t *imgerr;

	cookie = xcb_get_image(conn, XCB_IMAGE_FORMAT_Z_PIXMAP, screen->screen->root, 0, 0, w, h, -1);
	imgrep = xcb_get_image_reply(conn, cookie, &imgerr);
	if (imgerr || !imgrep) {
		err(1, "unable to get image %d", imgerr->error_code);
	}

	len = xcb_get_image_data_length(imgrep);
	tmp = malloc(len);

	src = (uint32_t*)xcb_get_image_data(imgrep);
	memcpy(tmp, src, len);

	apply_filters(tmp, w, h, conf.filters, conf.nfilter);

	screen->img.data = tmp;
	screen->img.len = len;

	free(imgrep);
	put_image(screen);
}

static struct screen_t *
init_screen(xcb_screen_t *xscreen) {
	xcb_void_cookie_t cookie;
	uint32_t mask = 0;
	uint32_t vals[12];
	struct screen_t *screen;

	if (!(screen = malloc(sizeof(struct screen_t)))) {
		return NULL;
	}

	screen->screen = xscreen;
	screen->mons = NULL;
	screen->nmon = 0;

	if (grab_inputs(screen->screen, 100) < 0) {
		errx(1, "failed to grab input devices");
	}
	
	set_monitors(screen);

	xcb_change_window_attributes(conn, screen->screen->root, XCB_CW_EVENT_MASK,
                                     (uint32_t[]){XCB_EVENT_MASK_STRUCTURE_NOTIFY});

	mask = XCB_GC_FOREGROUND
	     | XCB_GC_GRAPHICS_EXPOSURES;
	vals[0] = screen->screen->white_pixel;
	vals[1] = 0;

	screen->gc = xcb_generate_id(conn);
	cookie = xcb_create_gc_checked(conn,
	                               screen->gc,
	                               screen->screen->root,
	                               mask,
	                               vals);
	check_xcb_cookie(cookie, "could not create gc");

	mask = XCB_CW_OVERRIDE_REDIRECT
	     | XCB_CW_EVENT_MASK;
	vals[0] = 1;
	vals[1] = XCB_EVENT_MASK_EXPOSURE
		| XCB_EVENT_MASK_KEY_PRESS
		| XCB_EVENT_MASK_VISIBILITY_CHANGE
		| XCB_EVENT_MASK_STRUCTURE_NOTIFY;

	screen->win = xcb_generate_id(conn);
	cookie = xcb_create_window_checked(conn,
	                                   XCB_COPY_FROM_PARENT,
	                                   screen->win,
	                                   screen->screen->root,
	                                   0,
	                                   0,
	                                   screen->screen->width_in_pixels,
	                                   screen->screen->height_in_pixels,
	                                   0,
	                                   XCB_WINDOW_CLASS_INPUT_OUTPUT,
	                                   screen->screen->root_visual,
	                                   mask,
	                                   vals);
	check_xcb_cookie(cookie, "could not create window");
	xcb_flush(conn);

	screen->cmap = xcb_generate_id(conn);
	cookie = xcb_create_colormap_checked(conn,
	                                     XCB_COLORMAP_ALLOC_NONE,
	                                     screen->cmap,
	                                     screen->screen->root,
	                                     screen->screen->root_visual);
	check_xcb_cookie(cookie, "could not create colormap");

	init_colors(screen);

	if (rrbase >= 0) {
		mask = XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE;
		xcb_randr_select_input(conn, screen->screen->root, mask);
	}

	cookie = xcb_map_window_checked(conn, screen->win);
	check_xcb_cookie(cookie, "could not map window");

	create_image(screen);

	ksyms = xcb_key_symbols_alloc(conn);

	return screen;
}

void
xcb_init() {
	if (!(conn = xcb_connect(NULL, NULL))) {
		errx(1, "could not connect to X server");
	}

	const xcb_query_extension_reply_t *ext;
	ext = xcb_get_extension_data(conn, &xcb_randr_id);
	if (ext->present) {
		rrbase = ext->first_event;
	}

	xcb_screen_iterator_t iter;
	for (nscreens = 0, iter = xcb_setup_roots_iterator(xcb_get_setup(conn));
	     iter.rem;
	     ++nscreens, xcb_screen_next(&iter)) {
		if (!(screens = realloc(screens, (nscreens + 1) * sizeof(struct screen_t *)))) {
			err(1, "realloc\n");
		}
		if (!(screens[nscreens] = init_screen(iter.data))) {
			errx(1, "failed to screen screen %d", nscreens);
		}
	}
}

void
xcb_close() {
	int i;
	for (i = 0; i < nscreens; i++) {
		free(screens[i]->mons);
		free(screens[i]->img.data);
		free(screens[i]);
	}
	free(screens);

	xcb_key_symbols_free(ksyms);
	xcb_disconnect(conn);
}
