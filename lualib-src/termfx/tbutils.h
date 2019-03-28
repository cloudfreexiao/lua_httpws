/* termbox utils
 *
 * Utility functions for use with termbox
 *
 * Gunnar Zötl <gz@tset.de>, 2015
 * Released under the terms of the MIT license. See file LICENSE for details.
 */

#ifndef tbutils_h
#define tbutils_h

#define CELL(buf, w, x, y) (buf)[(y) * (w) + (x)]

/* blit one buffer into another buffer
 *
 * Arguments:
 *	to		target buffer to blit into
 *	tw, th	target buffer dimensions
 *	x, y	target coordinates
 *	from	source buffer, must be different from "to"
 *	w, h	dimensions of source buffer
 */
void tbu_blitbuffer(struct tb_cell *to, int tw, int th, int x, int y, const struct tb_cell *from, int w, int h);

/* blit buffer into terminal back buffer
 *
 * Arguments:
 *	x, y	target coordinates
 *	from	source buffer
 *	w, h	dimensions of source buffer
 */
void tbu_blit(int x, int y, const struct tb_cell *from, int w, int h);

/* fill a region of a buffer
 *
 * Arguments:
 *	buf		target buffer
 *	bw, bh	target buffer dimensions
 *	x, y	target coordinates
 *	w, h	dimensions of rect to fill
 *	fill	cell describing what to fill with
 */
void tbu_fillbufferregion(struct tb_cell *buf, int bw, int bh, int x, int y, int w, int h, const struct tb_cell *fill);

/* fill a region of the terminal back buffer
 *
 * Arguments:
 *	x, y	target coordinates
 *	w, h	dimensions of rect to fill
 *	fill	cell describing what to fill with
 */
void tbu_fillregion(int x, int y, int w, int h, const struct tb_cell *fill);

/* copy a region of a buffer to another place
 *
 * Arguments:
 *	buf		target buffer
 *	bw, bh	target buffer dimensions
 *	tx, ty	target coordinates
 *	x, y	source coordinates
 *	w, h	dimensions of rect to copy
 */
void tbu_copybufferregion(struct tb_cell *buf, int bw, int bh, int tx, int ty, int x, int y, int w, int h);

/* copy a region of the terminal back buffer to another place
 *
 * Arguments:
 *	tx, ty	target coordinates
 *	x, y	source coordinates
 *	w, h	dimensions of rect to copy
 */
void tbu_copyregion(int tx, int ty, int x, int y, int w, int h);

/* scroll a region within a buffer
 *
 * Arguments:
 *	buf		target buffer
 *	bw, bh	target buffer dimensions
 *	x, y	coordinates of scrolled region
 *	w, h	dimensions of scrolled region
 *	sx, sy	directions to scroll in x and y
 *	fill	what to fill the cleared space with
 *
 * Note: the amount by which is scrolled is always 1.
 * sy and sx only give the directions: -1 left/up, 0 none, 1 right/down.
 */
void tbu_scrollbufferregion(struct tb_cell *buf, int bw, int bh, int x, int y, int w, int h, int sx, int sy, const struct tb_cell *fill);

/* scroll a region within the terminal back buffer
 *
 * Arguments:
 *	x, y	coordinates of scrolled region
 *	w, h	dimensions of scrolled region
 *	sx, sy	directions to scroll in x and y
 *	fill	what to fill the cleared space with
 *
 * Note: the amount by which is scrolled is always 1.
 * sy and sx only give the directions: -1 left/up, 0 none, 1 right/down.
 */
void tbu_scrollregion(int x, int y, int w, int h, int sx, int sy, const struct tb_cell *fill);

#endif /* tbutils_h */
