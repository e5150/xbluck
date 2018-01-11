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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>

#include "xbluck.h"

static void
__check_param(bool valid, const char *fmt, ...) {
	if (!valid) {
		fprintf(stderr, "%s:", program_invocation_name);
		va_list ap;
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
		exit(1);
	}
}

#define CHECK_PARAM(valid, fmt, ...) \
__check_param(valid, "%s:" fmt "\n", __func__, ##__VA_ARGS__)

#define CLAMP(val) ((val) > 0xFF ? 0xFF : (val) < 0 ? 0 : (val))
#define CHANR(val) ((val >> 16) & 0xFF)
#define CHANG(val) ((val >>  8) & 0xFF)
#define CHANB(val) ((val >>  0) & 0xFF)
#define MKRGB(r, g, b) (((CLAMP(r) & 0xFF) << 16) | ((CLAMP(g) & 0xFF) <<  8) | ((CLAMP(b) & 0xFF) <<  0))

FILTERCHK(gaussian) {
	CHECK_PARAM(param.u >= 2, "radius=%u: must be ≥ 2", param.u);
	CHECK_PARAM(param.u < 30, "radius=%u: integer overflow", param.u);
}
FILTERFUNC(gaussian) {
	DEBUG(1, "img=%p w=%d h=%d r=%d", (void*)img, w, h, param.u);
	uint32_t *ins, *row;
	int64_t r, g, b;
	int x, y, dx, dy;
	int rad = param.u;
	int klen = 2 * rad + 1;
	uint32_t kern[klen];
	int i;

	uint32_t *tmp = malloc(w * h * sizeof(uint32_t));
	uint32_t div = 0;

	kern[0] = 1;
	for (i = 1; i < klen; ++i) {
		kern[i] = kern[i-1] * (klen - i) / i;
		div += kern[i];
	}

	for (ins = tmp, y = 0; y < h; y++) {
		row = img + y * w;
		for (x = 0; x < w; x++) {
			r = g = b = 0;
			for (dx = x - rad, i = 0; i < klen; ++i, ++dx) {
				if (dx < 0 || dx >= w)
					continue;

				r += CHANR(row[dx]) * kern[i];
				g += CHANG(row[dx]) * kern[i];
				b += CHANB(row[dx]) * kern[i];
			}
			r /= div;
			g /= div;
			b /= div;
			*ins++ = MKRGB(r, g, b);
		}
	}

	for (ins = img, y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			r = g = b = 0;
			for (dy = y - rad, i = 0; i < klen; ++i, ++dy) {
				if (dy < 0 || dy >= h)
					continue;
				
				row = tmp + dy * w;
				r += CHANR(row[x]) * kern[i];
				g += CHANG(row[x]) * kern[i];
				b += CHANB(row[x]) * kern[i];
			}
			r /= div;
			g /= div;
			b /= div;
			*ins++ = MKRGB(r, g, b);
		}
	}
	free(tmp);
}

FILTERCHK(pixelate) {
	CHECK_PARAM(param.u >= 2, "pixels=%u: must be ≥ 2", param.u);
}
FILTERFUNC(pixelate) {
	DEBUG(1, "img=%p w=%d h=%d siz=%d", (void*)img, w, h, param.u);
	int siz = param.u;
	uint32_t *row;
	int x, y, dy, dx;

	for (y = 0; y <= h - siz; y += siz)
	for (x = 0; x <= w - siz; x += siz) {
		int64_t r = 0, g = 0, b = 0;
		for (dy = y; dy < y + siz; ++dy) {
			row = img + dy * w;
			for (dx = x; dx < x + siz; ++dx) {
				r += CHANR(row[dx]);
				g += CHANG(row[dx]);
				b += CHANB(row[dx]);
			}
		}
		r /= siz * siz;
		g /= siz * siz;
		b /= siz * siz;
		for (dy = y; dy < y + siz; ++dy) {
			row = img + dy * w;
			for (dx = x; dx < x + siz; ++dx) {
				row[dx] = MKRGB(r, g, b);
			}
		}
	}
}

FILTERCHK(edge) {
	(void)param;
}
FILTERFUNC(edge) {
	DEBUG(1, "img=%p w=%d w=%d", (void*)img, w, h);
	(void)param;
	int x, y;
	uint8_t *grey, *dst;
	uint32_t *src;
	grey = malloc(w * h * sizeof(uint8_t));

	for (dst = grey, src = img, y = 0; y < h; ++y)
	for (x = 0; x < w; ++x, ++src, ++dst) {
		*dst = CLAMP(
		      CHANR(*src) * .30
		    + CHANG(*src) * .58
		    + CHANB(*src) * .12
		);
	}
	for (y = 1; y < h - 1; ++y) {
		uint8_t *rp = grey + (y - 1) * w;
		uint8_t *rc = rp + w;
		uint8_t *rn = rc + w;
		for (x = 1; x < w - 1; ++x) {
			int dx, dy;
			dx = abs((
			     -rp[x - 1]
			     -rp[x] * 2
			     -rp[x + 1]
			     +rn[x - 1]
			     +rn[x] * 2
			     +rn[x + 1]) / 8);
			dy = abs((
			     -rp[x - 1]
			     -rc[x - 1] * 2
			     -rn[x - 1]
			     +rp[x + 1]
			     +rc[x + 1] * 2
			     +rn[x + 1]) / 8);
			int avg = (dx + dy) / 2;
			img[y * w + x] = MKRGB(avg, avg, avg);

		}
	}
	free(grey);
}

FILTERCHK(tile) {
	CHECK_PARAM(param.us.u1 < 64, "htile=%u: Nonsensically large", param.us.u1);
	CHECK_PARAM(param.us.u2 < 64, "vtile=%u: Nonsensically large", param.us.u2);
	CHECK_PARAM(param.us.u1 != 0, "htile=%u: Must be non-zero", param.us.u1);
	CHECK_PARAM(param.us.u2 != 0, "vtile=%u: Must be non-zero", param.us.u2);
	CHECK_PARAM(param.us.u1 != 1 || param.us.u2 != 1,
	            "vtile=%u, htile=%u: Both cannot be one (1)", param.us.u1, param.us.u2);
}
FILTERFUNC(tile) {
	DEBUG(1, "img=%p w=%d w=%d Dx=%d Dy=%d", (void*)img, w, h, param.us.u1, param.us.u2);
	int x, y, dx, dy;
	int nw = param.us.u1;
	int nh = param.us.u2;
	int sw = w / nw;
	int sh = h / nh;
	int n = nw * nh;
	uint32_t *small = malloc(sw * sh * sizeof(uint32_t));
	uint32_t *p;

	p = small;
	for (y = 0; y <= h - nh; y += nh)
	for (x = 0; x <= w - nw; x += nw) {
		int64_t r = 0, g = 0, b = 0;
		for (dy = y; dy < y + nh; ++dy) {
			uint32_t *row = img + dy * w;
			for (dx = x; dx < x + nw; ++dx) {
				r += CHANR(row[dx]);
				g += CHANG(row[dx]);
				b += CHANB(row[dx]);
			}
		}
		*p++ = MKRGB(r / n, g / n, b / n);
	}

	for (dy = y = 0; y < h; ++y) {
		uint32_t *drow = img + y * w;
		uint32_t *srow = small + dy * sw;
		if (++dy == sh) dy = 0;
		for (dx = x = 0; x < w; ++x) {
			drow[x] = srow[dx];
			if (++dx == sw) dx = 0;
		}
	}

	free(small);
	
}

FILTERCHK(flip) {
	(void)param;
}
FILTERFUNC(flip) {
	DEBUG(1, "img=%p w=%d h=%d", (void*)img, w, h);
	(void)param;
	size_t len = w * sizeof(uint32_t);
	uint32_t *ph = malloc(len);
	uint32_t *hi, *lo;
	int y;
	for (y = 0; y < h / 2; ++y) {
		hi = img + w * y;
		lo = img + w * (h - y - 1);
		memcpy(ph, hi, len);
		memcpy(hi, lo, len);
		memcpy(lo, ph, len);
	}
	free(ph);
}

FILTERCHK(flop) {
	(void)param;
}
FILTERFUNC(flop) {
	DEBUG(1, "img=%p w=%d h=%d", (void*)img, w, h);
	(void)param;
	int y, x;
	for (y = 0; y < h; ++y) {
		uint32_t *row = img + w * y;
		for (x = 0; x < w / 2; ++x) {
			uint32_t tmp = row[x];
			row[x] = row[w - x - 1];
			row[w - x - 1] = tmp;
		}
	}
}

FILTERCHK(shift) {
	CHECK_PARAM(param.u, "pixels=%u:Must be non-zero", param.u);
}
FILTERFUNC(shift) {
	DEBUG(1, "img=%p w=%d w=%d n=%d", (void*)img, w, h, param.u);
	int y, x;
	int n = param.u;
	uint32_t *tmp = malloc(w * sizeof(*tmp));

	for (y = 0; y < h; ++y) {
		uint32_t *row = img + w * y;
		if (y % 2) {
			memcpy(tmp, row, n);
			for (x = 0; x < w - n - 1; ++x)
				row[x] = row[x + n];
			memcpy(row + w - n, tmp, n);
		} else {
			memcpy(tmp, row + w - n, n);
			for (x = w - n - 1; x > n; --x)
				row[x+n] = row[x];
			memcpy(row, tmp, n);
		}
	}
	free(tmp);
}

FILTERCHK(null) {
	(void)param;
}
FILTERFUNC(null) {
	DEBUG(1, "img=%p w=%d w=%d", (void*)img, w, h);
	(void)img;
	(void)w;
	(void)h;
	(void)param;
}

FILTERCHK(colourise) {
	(void)param;
}
FILTERFUNC(colourise) {
	DEBUG(1, "img=%p w=%d w=%d color=%08x", (void*)img, w, h, param.u);
	int x, y;
	double aa = ((param.u >> 24) & 0xFF) / 255.0;
	uint32_t rr = CHANR(param.u) * aa;
	uint32_t gg = CHANG(param.u) * aa;
	uint32_t bb = CHANB(param.u) * aa;

	for (y = 0; y < h; ++y)
	for (x = 0; x < w; ++x, ++img) {
		int64_t r = CHANR(*img) * aa + rr;
		int64_t g = CHANR(*img) * aa + gg;
		int64_t b = CHANR(*img) * aa + bb;
		
		*img = MKRGB(r, g, b);
	}
}

FILTERCHK(invert) {
	(void)param;
}
FILTERFUNC(invert) {
	DEBUG(1, "img=%p w=%d w=%d", (void*)img, w, h);
	(void)param;
	int x, y;
	for (y = 0; y < h; ++y)
	for (x = 0; x < w; ++x, ++img) {
		*img ^= 0xFFFFFF;
	}
}

FILTERCHK(noise) {
	CHECK_PARAM(param.u <= 0xFF, "noise=0x%04x:Must be <= 0xFF", param.u);
}
FILTERFUNC(noise) {
	DEBUG(1, "img=%p w=%d w=%d level=%02x", (void*)img, w, h, param.u);
	int n = param.u;
	int x, y;
	for (y = 0; y < h; ++y)
	for (x = 0; x < w; ++x, ++img) {
		uint32_t base = rand();
		int32_t r = CHANR(*img) + (CHANR(base) % n) * ((base & 0x01000000) ? 1 : -1);
		int32_t g = CHANG(*img) + (CHANG(base) % n) * ((base & 0x02000000) ? 1 : -1);
		int32_t b = CHANB(*img) + (CHANB(base) % n) * ((base & 0x04000000) ? 1 : -1);
		*img = MKRGB(r, g, b);
	}
}

FILTERCHK(greyscale) {
	(void)param;
}
FILTERFUNC(greyscale) {
	DEBUG(1, "img=%p w=%d w=%d", (void*)img, w, h);
	(void)param;
	int x, y;

	for (y = 0; y < h; ++y)
	for (x = 0; x < w; ++x, ++img) {
		int64_t pix = CLAMP(
		      CHANR(*img) * .30
		    + CHANG(*img) * .58
		    + CHANB(*img) * .12
		);
		*img = MKRGB(pix, pix, pix);
	}
}

void
apply_filters(uint32_t *img, int w, int h, struct filter_t *filters, int n) {
	int i;
	for (i = 0; i < n; ++i) {
		if (!filters[i].function)
			break;
		filters[i].function(img, w, h, filters[i].param);
	}
}

