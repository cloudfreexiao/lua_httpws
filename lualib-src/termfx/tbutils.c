/* termbox utils
 *
 * Utility functions for use with termbox
 *
 * Gunnar Zötl <gz@tset.de>, 2015
 * Released under the terms of the MIT license. See file LICENSE for details.
 */

#include <string.h>
#include <stdlib.h>

#include "termbox.h"
#include "tbutils.h"

void tbu_blitbuffer(struct tb_cell *to, int tw, int th, int x, int y, const struct tb_cell *from, int w, int h)
{
        if (x + w < 0 || x >= tw || w <= 0)
                return;
        if (y + h < 0 || y >= th || h <= 0)
                return;
        int xo = 0, yo = 0, ww = w, hh = h;
        if (x < 0) {
                xo = -x;
                ww -= xo;
                x = 0;
        }
        if (y < 0) {
                yo = -y;
                hh -= yo;
                y = 0;
        }
        if (ww > tw - x) {
        	ww = tw - x;
        }
        if (hh > th - y) {
            hh = th - y;
    	}
        int sy;
        struct tb_cell *dst = &CELL(to, tw, x, y);
        const struct tb_cell *src = from + yo * w + xo;
        size_t size = sizeof(struct tb_cell) * ww;

        for (sy = 0; sy < hh; ++sy) {
                memcpy(dst, src, size);
                dst += tw;
                src += w;
        }
}

void tbu_blit(int x, int y, const struct tb_cell *from, int w, int h)
{
	int bbw = tb_width();
	int bbh = tb_height();
	struct tb_cell *bb = tb_cell_buffer();

	tbu_blitbuffer(bb, bbw, bbh, x, y, from, w, h);
}

void tbu_fillbufferregion(struct tb_cell *buf, int bw, int bh, int x, int y, int w, int h, const struct tb_cell *fill)
{
    if (x + w < 0 || x >= bw || w <= 0)
            return;
    if (y + h < 0 || y >= bh || h <= 0)
            return;

	if (x < 0) {
		w += x;
		x = 0;
	}
	if (y < 0) {
		h += y;
		y = 0;
	}
	if (x + w > bw) {
		w = bw - x;
	}
	if (y + h > bh) {
		h = bh - y;
	}
	int sx, sy;
    struct tb_cell *dst = &CELL(buf, bw, x, y);
    for (sy = 0; sy < h; ++sy) {
    	for (sx = 0; sx < w; ++sx) {
    		dst[sx] = *fill;
    	}
    	dst += bw;
    }
}

void tbu_fillregion(int x, int y, int w, int h, const struct tb_cell *fill)
{
	int bbw = tb_width();
	int bbh = tb_height();
	struct tb_cell *bb = tb_cell_buffer();

	tbu_fillbufferregion(bb, bbw, bbh, x, y, w, h, fill);
}

void tbu_copybufferregion(struct tb_cell *buf, int bw, int bh, int tx, int ty, int x, int y, int w, int h)
{
    if (w < 1 || h < 1)
        return;
    if (x >= bw || x + w < 0 || y >= bh || y + h < 0)
        return;
    if (tx >= bw || tx + w < 0 || ty >= bh || ty + h < 0)
        return;

    if (x < 0) {
        int dx = -x;
        x = 0;
        tx += dx;
        w -= dx;
    }
    if (x + w > bw) {
        w = bw - x;
    }
    if (tx < 0) {
        int dx = -tx;
        tx = 0;
        x += dx;
        w -= dx;
    }
    if (tx + w > bw) {
        w = bw - tx;
    }

    if (y < 0) {
        int dy = -y;
        y = 0;
        ty += dy;
        h -= dy;
    }
    if (y + h > bh) {
        h = bh - y;
    }
    if (ty < 0) {
        int dy = -ty;
        ty = 0;
        y += dy;
        h -= dy;
    }
    if (ty + h > bh) {
        h = bh - ty;
    }

    int ys = 1;

    if (ty > y) {
        y = y + h - 1;
        ty = ty + h - 1;
        ys = -1;
    }

    int ry;
    int from = x, to = tx;
    if (y > 0) {
        from += y * bw;
    }
    if (ty > 0) {
        to += ty * bw;
    }

    for (ry = 0; ry < h; ++ry) {
        int cfy = from + (ry * ys * bw);
        int cty = to + (ry * ys * bw);
        memmove(&buf[cty], &buf[cfy], w * sizeof(struct tb_cell));
    }
}

void tbu_copyregion(int tx, int ty, int x, int y, int w, int h)
{
	int bbw = tb_width();
	int bbh = tb_height();
	struct tb_cell *bb = tb_cell_buffer();

	tbu_copybufferregion(bb, bbw, bbh, tx, ty, x, y, w, h);
}

void tbu_scrollbufferregion(struct tb_cell *buf, int bw, int bh, int x, int y, int w, int h, int sx, int sy, const struct tb_cell *fill)
{
    int fx = x, tx = x;
    int fy = y, ty = y;

    if (sx < 0) {
        sx = -1;
        fx = x + 1;
    } else if (sx > 0) {
        sx = 1;
        tx = x + 1;
    }
    
    if (sy < 0) {
        sy = -1;
        fy = y + 1;
    } else if (sy > 0) {
        sy = 1;
        ty = y + 1;
    }

    tbu_copybufferregion(buf, bw, bh, tx, ty, fx, fy, w - abs(sx), h - abs(sy));
    if (sx != 0) {
        int fillx = sx > 0 ? x : x + w - 1;
        tbu_fillbufferregion(buf, bw, bh, fillx, y, 1, h, fill);
    }
    if (sy != 0) {
        int filly = sy > 0 ? y : y + h - 1;
        tbu_fillbufferregion(buf, bw, bh, x, filly, w, 1, fill);
    }
}

void tbu_scrollregion(int x, int y, int w, int h, int sx, int sy, const struct tb_cell *fill)
{
	int bbw = tb_width();
	int bbh = tb_height();
	struct tb_cell *bb = tb_cell_buffer();

	tbu_scrollbufferregion(bb, bbw, bbh, x, y, w, h, sx, sy, fill);
}
