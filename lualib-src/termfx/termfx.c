/* termfx.c
 *
 * provide simple terminal interface for lua
 *
 * Gunnar Zötl <gz@tset.de>, 2014-2015
 * Released under the terms of the MIT license. See file LICENSE for details.
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>

#include "lua.h"
#include "lauxlib.h"

#include "mini_utf8.h"

#include "termbox.h"
#include "tbutils.h"
#include "termfx.h"

/* userdata for a termbox cell */
typedef struct tb_cell TfxCell;

/* userdata for a termfx offscreen buffer */
typedef struct {
	int w, h;
	uint16_t fg, bg;
	struct tb_cell *buf;
} TfxBuffer;

static const int top_left_coord = 1;
static uint16_t default_fg, default_bg;
static int initialized = 0;

static uint64_t mstimer_tm0 = 0;

/* helper: try to get a char from the stack at index. If the value there
 * is a number, return it. If the value is a string of length 1, return
 * the value of the first char. If it is a string of length >1, try to
 * read it as an utf8 encoded char.
 */
static uint32_t _tfx_getchar(lua_State *L, int index)
{
	uint32_t res = 0;
	int t = lua_type(L, index);
	if (t == LUA_TNUMBER)
		res = (uint32_t) lua_tointeger(L, index);
	else if (t == LUA_TSTRING) {
		const char* str = lua_tostring(L, index);
		int l = strlen(str);
		if (l == 1)
			res = *str;
		else if (l > 1) {
			res = mini_utf8_decode(&str);
			if (*str != 0)
				return luaL_argerror(L, index, "invalid char");
		}
	} else {
		return luaL_argerror(L, index, "number or string");
	}
	return res;
}

/* helper: as above, but if there's nil or nothing on the stack at the
 * index, return the default value.
 */
static uint32_t _tfx_optchar(lua_State *L, int index, uint32_t dfl)
{
	if (!lua_isnoneornil(L, index))
		return _tfx_getchar(L, index);
	return dfl;
}

/* helper: check whether the value of the given index on the lua stack
 * is a userdata of the given type. Return 1 for yes, 0 for no.
 */
static int _tfx_isUserdataOfType(lua_State *L, int index, const char* type)
{
	if (lua_isuserdata(L, index)) {
		if (lua_getmetatable(L, index)) {
			luaL_getmetatable(L, type);
			if (lua_rawequal(L, -1, -2)) {
				lua_pop(L, 2);
				return 1;
			}
			lua_pop(L, 2);
		}
	}
	return 0;
}

/* helper: return a millisecond timer, which is 0 at the load time of
 * the library, so that the result will fit into an uint32_t for some time
 * (a little more than 49 days)
 */
static uint32_t _tfx_getMsTimer(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	uint64_t tm = tv.tv_sec * 1000 + tv.tv_usec / 1000 - mstimer_tm0;
	if (tm > INT_MAX) {
		mstimer_tm0 += INT_MAX;
		tm -= INT_MAX;
	}
	return (uint32_t) tm;
}

/*** TfxCell Userdata handling ***/

/* tfx_isCell
 *
 * If the value at the given acceptable index is a full userdata of the
 * type TFXCELL, then return 1, else return 0.
 *
 * Arguments:
 * 	L	Lua State
 *	index	stack index where the userdata is expected
 */
static int tfx_isCell(lua_State *L, int index)
{
	return _tfx_isUserdataOfType(L, index, TFXCELL);
}

/* tfx_toCell
 *
 * If the value at the given acceptable index is a full userdata, returns
 * its block address. Otherwise, returns NULL. 
 *
 * Arguments:
 * 	L	Lua State
 *	index	stack index where the userdata is expected
 */
static TfxCell* tfx_toCell(lua_State *L, int index)
{
	TfxCell *tfxcell = (TfxCell*) lua_touserdata(L, index);
	return tfxcell;
}

/* tfx_checkCell
 *
 * Checks whether the function argument narg is a userdata of the type
 * TFXCELL. If so, returns its block address, else throw an error.
 *
 * Arguments:
 * 	L	Lua State
 *	index	stack index where the userdata is expected
 */
static TfxCell* tfx_checkCell(lua_State *L, int index)
{
	TfxCell *tfxcell = (TfxCell*) luaL_checkudata(L, index, TFXCELL);
	return tfxcell;
}

/* tfx_pushCell
 *
 * create a new, empty TfxCell userdata and push it to the stack.
 *
 * Arguments:
 *	L	Lua state
 */
static TfxCell* tfx_pushCell(lua_State *L)
{
	TfxCell *tfxcell = (TfxCell*) lua_newuserdata(L, sizeof(TfxCell));
	luaL_getmetatable(L, TFXCELL);
	lua_setmetatable(L, -2);
	return tfxcell;
}

/* tfx__toStringCell
 *
 * __tostring metamethod for the TfxCell userdata.
 * Returns a string representation of the TfxCell
 *
 * Arguments:
 *	L	Lua State
 *
 * Lua Stack:
 *	1	TfxCell userdata
 * 
 * Lua Returns:
 * 	+1	the string representation of the TfxCell userdata
 */
static int tfx__tostringCell(lua_State *L)
{
	TfxCell *tfxcell = (TfxCell*) lua_touserdata(L, 1);
	lua_pushfstring(L, "%s (%p)", TFXCELL, tfxcell);
	return 1;
}

/* tfx__indexCell
 *
 * __index metamethod for the TfxCell userdata.
 * Returns a the value accessible at the index which is on top of the
 * stack, or nil of there is no such value
 *
 * Arguments:
 *	L	Lua State
 *
 * Lua Stack:
 *	1	TfxCell userdata
 * 	2	index to access
 * 
 * Lua Returns:
 * 	+1	value accessible through index
 */
static int tfx__indexCell(lua_State *L)
{
	TfxCell *tfxcell = tfx_checkCell(L, 1);
	const char* what = luaL_checkstring(L, 2);
	
	if (!strcmp(what, "ch"))
		lua_pushinteger(L, tfxcell->ch);
	else if (!strcmp(what, "fg"))
		lua_pushinteger(L, tfxcell->fg);
	else if (!strcmp(what, "bg"))
		lua_pushinteger(L, tfxcell->bg);
	else
		lua_pushnil(L);
	return 1;
}

/* tfx__newindexCell
 *
 * __newindex metamethod for the TfxCell userdata.
 * Sets a the value accessible at the index which is on the stack.
 *
 * Arguments:
 *	L	Lua State
 *
 * Lua Stack:
 *	1	TfxCell userdata
 * 	2	index to set
 * 	3	value to set
 * 
 * Lua Returns:
 * 	-
 */
static int tfx__newindexCell(lua_State *L)
{
	TfxCell *tfxcell = tfx_checkCell(L, 1);
	const char* what = luaL_checkstring(L, 2);
	uint32_t val = (uint32_t) luaL_checkinteger(L, 3);

	if (!strcmp(what, "ch"))
		tfxcell->ch = val < ' ' ? ' ' : val;
	else if (!strcmp(what, "fg"))
		tfxcell->fg = val;
	else if (!strcmp(what, "bg"))
		tfxcell->bg = val;
	return 0;
}

/* tfx_newCell
 *
 * create a new TfxCell object, initialize it, put it into a userdata and
 * return it to the user.
 *
 * Arguments:
 *	L	Lua State
 *
 * Lua Stack:
 *	+1	(opt) char to store, defaults to ' '
 * 	+2	(opt) forgeround color, defaults to tfx default foreground color
 * 	+3	(opt) background color, defaults to tfx default background color
 *
 * Lua Returns:
 *	+1	the TfxCell userdata
 */
static int tfx_newCell(lua_State *L)
{
	maxargs(L, 3);
	uint32_t ch = _tfx_optchar(L, 1, ' ');
	uint16_t fg = (uint16_t) luaL_optinteger(L, 2, default_fg) & 0xFFFF;
	uint16_t bg = (uint16_t) luaL_optinteger(L, 3, default_bg) & 0xFFFF;
	TfxCell *tfxcell = tfx_pushCell(L);
	tfxcell->ch = ch < ' ' ? ' ' : ch;
	tfxcell->fg = fg;
	tfxcell->bg = bg;
	return 1;
}

/* metamethods for the TfxCell userdata
 */
static const luaL_Reg tfx_CellMeta[] = {
	{"__tostring", tfx__tostringCell},
	{"__index",  tfx__indexCell},
	{"__newindex", tfx__newindexCell},
	{NULL, NULL}
};

/*** TfxBuffer Userdata handling ***/

/* tfx_isBuffer
 *
 * If the value at the given acceptable index is a full userdata of the
 * type TFXBUFFER, then return 1, else return 0.
 *
 * Arguments:
 * 	L	Lua State
 *	index	stack index where the userdata is expected
 */
static int tfx_isBuffer(lua_State *L, int index)
{
	return _tfx_isUserdataOfType(L, index, TFXBUFFER);
}

/* tfx_checkBuffer
 *
 * Checks whether the function argument narg is a userdata of the type
 * TFXBUFFER. If so, returns its block address, else throw an error.
 *
 * Arguments:
 * 	L	Lua State
 *	index	stack index where the userdata is expected
 */
static TfxBuffer* tfx_checkBuffer(lua_State *L, int index)
{
	TfxBuffer *tfxbuf = (TfxBuffer*) luaL_checkudata(L, index, TFXBUFFER);
	return tfxbuf;
}

/* tfx_pushBuffer
 *
 * create a new, empty TfxBuffer userdata and push it to the stack.
 *
 * Arguments:
 *	L	Lua state
 */
static TfxBuffer* tfx_pushBuffer(lua_State *L)
{
	TfxBuffer *tfxbuf = (TfxBuffer*) lua_newuserdata(L, sizeof(TfxBuffer));
	luaL_getmetatable(L, TFXBUFFER);
	lua_setmetatable(L, -2);
	return tfxbuf;
}

/*** Housekeeping metamethods ***/

/* tfx__gcBuffer
 *
 * __gc metamethod for the TfxBuffer userdata.
 *
 * Arguments:
 *	L	Lua State
 *
 * Lua Stack:
 *	1	TfxBuffer userdata
 */
static int tfx__gcBuffer(lua_State *L)
{
	TfxBuffer *tfxbuf = (TfxBuffer*) lua_touserdata(L, 1);
	if (tfxbuf->buf) free(tfxbuf->buf);
	return 0;
}

/* tfx__tostringBuffer
 *
 * __tostring metamethod for the TfxBuffer userdata.
 * Returns a string representation of the TfxBuffer
 *
 * Arguments:
 *	L	Lua State
 *
 * Lua Stack:
 *	1	TfxBuffer userdata
 *
 * Lua Returns:
 * 	+1	the string representation of the TfxCell userdata
 */
static int tfx__tostringBuffer(lua_State *L)
{
	TfxBuffer *tfxbuf = (TfxBuffer*) lua_touserdata(L, 1);
	lua_pushfstring(L, "%s (%p)", TFXBUFFER, tfxbuf);
	return 1;
}

/* metamethods for the TfxBuffer userdata
 */
static const luaL_Reg tfx_BufferMeta[] = {
	{"__gc", tfx__gcBuffer},
	{"__tostring", tfx__tostringBuffer},
	{NULL, NULL}
};

/* tfx_newBuffer
 *
 * create a new TfxBuffer object, initialize it, put it into a userdata
 * and return it to the user.
 *
 * Arguments:
 *	L	Lua State
 *
 * Lua Stack:
 *	-
 *
 * Lua Returns:
 *	+1	the TfxBuffer userdata
 */
static int tfx_newBuffer(lua_State *L)
{
	maxargs(L, 2);
	int w = (int) luaL_checkinteger(L, 1);
	int h = (int) luaL_checkinteger(L, 2);
	if (w < 1 || h < 1)
		return luaL_error(L, "buffer dimensions must be >=1");
	TfxBuffer *tfxbuf = tfx_pushBuffer(L);
	tfxbuf->buf = calloc(w * h, sizeof(struct tb_cell));
	tfxbuf->w = w;
	tfxbuf->h = h;
	tfxbuf->fg = default_fg;
	tfxbuf->bg = default_bg;
	return 1;
}

/* tfx_bufAttributes
 *
 * sets or gets default foreground and background attributes on a buffer.
 *
 * Arguments:
 *	L	Lua State
 *
 * Lua Stack:
 *	1	TfxBuffer userdata
 * 	2	(opt) foreground attributes
 * 	3	(opt) background attributes
 *
 * Lua Returns:
 *	+1	buffer default foreground attribute
 * 	+2	buffer default background attribute
 */
static int tfx_bufAttributes(lua_State *L)
{
	maxargs(L, 3);
	TfxBuffer *tfxbuf = tfx_checkBuffer(L, 1);
	if (lua_gettop(L) > 1) {
		tfxbuf->fg = (uint16_t) luaL_optinteger(L, 2, tfxbuf->fg) & 0xFFFF;
		tfxbuf->bg = (uint16_t) luaL_optinteger(L, 3, tfxbuf->bg) & 0xFFFF;
	}
	lua_pushinteger(L, tfxbuf->fg);
	lua_pushinteger(L, tfxbuf->bg);
	return 2;
}

/* tfx_bufClear
 *
 * clears a buffer to the default foreground and background attributes.
 * If the optional foreground and background attribute arguments are
 * given, these will be set using tfx_bufSetAttributes before clearing.
 *
 * Arguments:
 *	L	Lua State
 *
 * Lua Stack:
 *	1	TfxBuffer userdata
 * 	2	(opt) foreground attributes, defaults to buffer default foreground
 * 	3	(opt) background attributes, defaults to buffer default background
 *
 * Lua Returns:
 *	-
 */
static int tfx_bufClear(lua_State *L)
{
	if (lua_gettop(L) > 1)
		tfx_bufAttributes(L);

	TfxBuffer *tfxbuf = tfx_checkBuffer(L, 1);
	int c;
	for (c = 0; c < tfxbuf->w * tfxbuf->h; ++c) {
		TfxCell *cell = &tfxbuf->buf[c];
		cell->fg = tfxbuf->fg;
		cell->bg = tfxbuf->bg;
		cell->ch = ' ';
	}
	return 0;
}

/* _tfx_bufChangeCell
 * 
 * analog to tb_changecell() for buffers
 */
static int _tfx_bufChangeCell(TfxBuffer *tfxbuf, int x, int y, TfxCell *c)
{
	if (x > tfxbuf->w || y > tfxbuf->h || x < 0 || y < 0)
		return 0;
	
	TfxCell *cell = &tfxbuf->buf[y * tfxbuf->w + x];
	*cell = *c;
	return 1;
}

/* tfx_bufSetcell
 *
 * sets a cell in a buffer. Either uses the default values for foreground
 * and background, or the values passed on the lua stack.
 *
 * Arguments:
 *	L	Lua State
 *
 * Lua Stack:
 *	1	TfxBuffer userdata
 * 	2	x coordinate of cell
 * 	3	y coordinate of cell
 * either
 * 	4	TfxCell
 * or
 * 	4	(opt) char, defaults to ' '
 * 	5	(opt) foreground attributes, defaults to buffer default foreground
 * 	6	(opt) background attributes, defaults to buffer default background
 *
 * Lua Returns:
 *	-
 */
static int tfx_bufSetcell(lua_State *L)
{
	TfxBuffer *tfxbuf = tfx_checkBuffer(L, 1);
	int x = (int) luaL_checkinteger(L, 2) - top_left_coord;
	int y = (int) luaL_checkinteger(L, 3) - top_left_coord;
	
	uint32_t ch = ' ';
	uint16_t fg = TB_WHITE, bg = TB_BLACK;
	if (tfx_isCell(L, 4)) {
		maxargs(L, 4);
		TfxCell *tfxcell = tfx_toCell(L, 4);
		ch = tfxcell->ch;
		fg = tfxcell->fg;
		bg = tfxcell->bg;
	} else {
		maxargs(L, 6);
		ch = _tfx_optchar(L, 4, ' ');
		fg = (uint16_t) luaL_optinteger(L, 5, tfxbuf->fg) & 0xFFFF;
		bg = (uint16_t) luaL_optinteger(L, 6, tfxbuf->bg) & 0xFFFF;
	}
	TfxCell cell;
	cell.ch = ch < '?' ? ' ' : ch;
	cell.fg = fg;
	cell.bg = bg;
	
	_tfx_bufChangeCell(tfxbuf, x, y, &cell);
	return 0;
}

/* tfx_bufGetCell
 *
 * gets data from a cell in a buffer.
 *
 * Arguments:
 *	L	Lua State
 *
 * Lua Stack:
 *	1	TfxBuffer userdata
 * 	2	x coordinate of cell
 * 	3	y coordinate of cell
 *
 * Lua Returns:
 *	a TfxCell with the data from the read cell
 */
int tfx_bufGetCell(lua_State *L)
{
	TfxBuffer *tfxbuf = tfx_checkBuffer(L, 1);
	int x = (int) luaL_checkinteger(L, 2) - top_left_coord;
	int y = (int) luaL_checkinteger(L, 3) - top_left_coord;

	if (x < 0 || x >= tfxbuf->w || y < 0 || y >= tfxbuf->h)	{
		lua_pushnil(L);
		return 1;
	}
	TfxCell *tfxcell = tfx_pushCell(L);
	*tfxcell = CELL(tfxbuf->buf, tfxbuf->w, x, y);

	return 1;
}

/* tfx_bufBlit
 *
 * blits the contents of a buffer to another buffer. Just a modified
 * copy of my improved tb_blit() routine.
 *
 * Arguments:
 *	L	Lua State
 *
 * Lua Stack:
 * 	1	destination TfxBuffer
 * 	2	x coordinate of cell
 * 	3	y coordinate of cell
 * 	4	source TfxBuffer
 *
 * Lua Returns:
 *	-
 */
static int tfx_bufBlit(lua_State *L)
{
	TfxBuffer *tfxbuf = tfx_checkBuffer(L, 1);
	int x = (int) luaL_checkinteger(L, 2) - top_left_coord;
	int y = (int) luaL_checkinteger(L, 3) - top_left_coord;
	TfxBuffer *other = tfx_checkBuffer(L, 4);

	tbu_blitbuffer(tfxbuf->buf, tfxbuf->w, tfxbuf->h, x, y, other->buf, other->w, other->h);
	return 0;
}

/* tfx_bufRect
 *
 * draws a rectangle. Either uses the default values for foreground and
 * background, or the values passed on the lua stack.
 * 
 * Arguments:
 *	L	Lua State
 *
 * Lua Stack:
 *	1	TfxBuffer userdata
 * 	2	x coordinate of rect
 * 	3	y coordinate of rect
 * 	4	width of rect
 * 	5	height of rect
 * either
 * 	6	TfxCell
 * or
 * 	6	(opt) char, defaults to ' '
 * 	7	(opt) foreground attributes, defaults to buffer default foreground
 * 	8	(opt) background attributes, defaults to buffer default background
 *
 * Lua Returns:
 *	false if the rectangle was entirely outside of the terminal, true if not
 */
static int tfx_bufRect(lua_State *L)
{
	TfxCell cel, *celp = &cel;
	TfxBuffer *tfxbuf = tfx_checkBuffer(L, 1);
	int x = (int) luaL_checkinteger(L, 2) - top_left_coord;
	int y = (int) luaL_checkinteger(L, 3) - top_left_coord;
	int w = (int) luaL_checkinteger(L, 4);
	int h = (int) luaL_checkinteger(L, 5);
	if (tfx_isCell(L, 6)) {
		maxargs(L, 6);
		celp = tfx_toCell(L, 6);
	} else {
		maxargs(L, 8);
		cel.ch = _tfx_optchar(L, 6, 0);
		cel.fg = (uint16_t) luaL_optinteger(L, 7, default_fg) & 0xFFFF;
		cel.bg = (uint16_t) luaL_optinteger(L, 8, default_bg) & 0xFFFF;
	}

	tbu_fillbufferregion(tfxbuf->buf, tfxbuf->w, tfxbuf->h, x, y, w, h, celp);
	return 0;
}

/* tfx_bufCopyRegion
 * 
 * copies a rectangular region of a buffer to another place
 *
 * Arguments:
 *	L	Lua State
 *
 * Lua Stack
 *	1	TfxBuffer userdata
 * 	2	target x coordinate for copy
 * 	3	target y coordinate for copy
 * 	4	source x coordinate
 * 	5	source y coordinate
 *	6	width of rectangle to copy
 *	7	height of rectangle to copy
 *
 * Lua Returns:
 *	-
 */
static int tfx_bufCopyRegion(lua_State *L)
{
	TfxBuffer *tfxbuf = tfx_checkBuffer(L, 1);
	int tx = (int) luaL_checkinteger(L, 2) - top_left_coord;
	int ty = (int) luaL_checkinteger(L, 3) - top_left_coord;
	int x = (int) luaL_checkinteger(L, 4) - top_left_coord;
	int y = (int) luaL_checkinteger(L, 5) - top_left_coord;
	int w = (int) luaL_checkinteger(L, 6);
	int h = (int) luaL_checkinteger(L, 7);

	tbu_copybufferregion(tfxbuf->buf, tfxbuf->w, tfxbuf->h, tx, ty, x, y, w, h);
	return 0;
}

/* tfx_bufScrollRegion
 * 
 * scrolls a rectangular region of a buffer
 *
 * Arguments:
 *	L	Lua State
 *
 * Lua Stack
 *	1	TfxBuffer userdata
 * 	2	x coordinate of rectangle to scroll
 * 	3	y coordinate of rectangle to scroll
 *	4	width of rectangle to scroll
 *	5	height of rectangle to scroll
 *	6	(opt) x scroll direction (-1, 0, 1), defaults to 0
 *	7	(opt) y scroll direction (-1, 0, 1), defaults to 0
 * either
 * 	8	TfxCell
 * or
 * 	8	(opt) char, defaults to ' '
 * 	9	(opt) foreground attributes, defaults to buffer default foreground
 * 	10	(opt) background attributes, defaults to buffer default background
 *
 * Lua Returns:
 *	-
 *
 * Note:
 *	arguments 6 and 7 (scroll directions) are ints, but only their sign is
 *	used. Scrolling is always by at most 1 cell.
 */
int tfx_bufScrollRegion(lua_State *L)
{
	if (!initialized) return 0;
	
	TfxCell cel, *celp = &cel;

	TfxBuffer *tfxbuf = tfx_checkBuffer(L, 1);
	int x = (int) luaL_checkinteger(L, 2) - top_left_coord;
	int y = (int) luaL_checkinteger(L, 3) - top_left_coord;
	int w = (int) luaL_checkinteger(L, 4);
	int h = (int) luaL_checkinteger(L, 5);
	int sx = (int) luaL_optinteger(L, 6, 0);
	int sy = (int) luaL_optinteger(L, 7, 0);

	if (tfx_isCell(L, 8)) {
		maxargs(L, 8);
		celp = tfx_toCell(L, 8);
	} else {
		maxargs(L, 10);
		cel.ch = _tfx_optchar(L, 8, 0);
		cel.fg = (uint16_t) luaL_optinteger(L, 9, default_fg) & 0xFFFF;
		cel.bg = (uint16_t) luaL_optinteger(L, 10, default_bg) & 0xFFFF;
	}

	tbu_scrollbufferregion(tfxbuf->buf, tfxbuf->w, tfxbuf->h, x, y, w, h, sx, sy, celp);
	return 0;
}

/* tfx_bufWidth
 * 
 * returns the width of the buffer
 * 
 * Arguments:
 * 	L	Lua State
 * 
 * Lua Stack:
 * 	1	TfxBuffer userdata
 * 
 * Lua Returns:
 * 	+1	width of buffer
 */
static int tfx_bufWidth(lua_State *L)
{
	maxargs(L, 1);
	TfxBuffer *tfxbuf = tfx_checkBuffer(L, 1);
	lua_pushinteger(L, tfxbuf->w);
	return 1;
}

/* tfx_bufHeight
 * 
 * returns the height of the buffer
 * 
 * Arguments:
 * 	L	Lua State
 * 
 * Lua Stack:
 * 	1	TfxBuffer userdata
 * 
 * Lua Returns:
 * 	+1	height of buffer
 */
static int tfx_bufHeight(lua_State *L)
{
	maxargs(L, 1);
	TfxBuffer *tfxbuf = tfx_checkBuffer(L, 1);
	lua_pushinteger(L, tfxbuf->h);
	return 1;
}

/* tfx_bufSize
 * 
 * returns width and height of the buffer
 * 
 * Arguments:
 * 	L	Lua State
 * 
 * Lua Stack:
 * 	1	TfxBuffer userdata
 * 
 * Lua Returns:
 * 	+1	width of buffer
 * 	+2	height of buffer
 */
static int tfx_bufSize(lua_State *L)
{
	maxargs(L, 1);
	TfxBuffer *tfxbuf = tfx_checkBuffer(L, 1);
	lua_pushinteger(L, tfxbuf->w);
	lua_pushinteger(L, tfxbuf->h);
	return 2;
}

/*** termfx stuff ***/

/* tfx_init
 * 
 * initialize TermFX
 * 
 * Arguments:
 * 	L	Lua State
 * 
 * Lua Stack:
 * 	1	(opt) boolean value, if true, then all coordinates will be
 * 		0-based, otherwise they will be 1-based.
 * 
 * Lua Returns:
 * 	-
 */
static int tfx_init(lua_State *L)
{
	maxargs(L, 0);
	default_fg = TB_WHITE;
	default_bg = TB_BLACK;
	
	int status = tb_init();
	if (status < 0) {
		switch(status) {
			case TB_EUNSUPPORTED_TERMINAL: return luaL_error(L, "unsupported terminal");
			case TB_EFAILED_TO_OPEN_TTY: return luaL_error(L, "failed to open tty");
			case TB_EPIPE_TRAP_ERROR: return luaL_error(L, "sigpipe trap");
			default: return luaL_error(L, "unknown error");
		}
	} else
		initialized = 1;
	
	return 0;
}

/* helper: shutdown termfx. Also used for atexit.
 */
static void _tfx_doShutdown(void)
{
	if (initialized)
		tb_shutdown();
	initialized = 0;
}

/* tfx_shutdown
 * 
 * shutdown TermFX
 * 
 * Arguments:
 * 	L	Lua State
 * 
 * Lua Stack:
 * 	-
 * 
 * Lua Returns:
 * 	-
 */
static int tfx_shutdown(lua_State *L)
{
	maxargs(L, 0);
	_tfx_doShutdown();
	return 0;
}

/* tfx_width
 * 
 * returns width of terminal
 * 
 * Arguments:
 * 	L	Lua State
 * 
 * Lua Stack:
 * 	-
 * 
 * Lua Returns:
 * 	+1	width of terminal, or nil if the terminal is not initialized.
 */
static int tfx_width(lua_State *L)
{
	maxargs(L, 0);
	int w = tb_width();
	if (w >= 0)
		lua_pushinteger(L, w);
	else
		lua_pushnil(L);
	return 1;
}

/* tfx_height
 * 
 * returns height of terminal
 * 
 * Arguments:
 * 	L	Lua State
 * 
 * Lua Stack:
 * 	-
 * 
 * Lua Returns:
 * 	+1	height of terminal, or nil if the terminal is not initialized.
 */
static int tfx_height(lua_State *L)
{
	maxargs(L, 0);
	int h = tb_height();
	if (h >= 0)
		lua_pushinteger(L, h);
	else
		lua_pushnil(L);
	return 1;
}

/* tfx_size
 * 
 * returns width and height of terminal
 * 
 * Arguments:
 * 	L	Lua State
 * 
 * Lua Stack:
 * 	-
 * 
 * Lua Returns:
 * 	+1	width of terminal, or nil if the terminal is not initialized.
 * 	+2	height of terminal, or nil if the terminal is not initialized.
 */
static int tfx_size(lua_State *L)
{
	maxargs(L, 0);
	int w = tb_width(), h = tb_height();
	if (w >= 0) {
		lua_pushinteger(L, w);
		lua_pushinteger(L, h);
	} else {
		lua_pushnil(L);
		lua_pushnil(L);
	}
	return 2;
}

/* tfx_attributes
 * 
 * sets and gets default foreground and background attributes for terminal
 * operations
 * 
 * Arguments:
 * 	L	Lua State
 * 
 * Lua Stack:
 * 	1	(opt) foreground attribute
 * 	2	(opt) background attribute
 * 
 * Lua Returns:
 *	+1	default foreground attribute, or nothing if terminal is not initialized
 * 	+2	default background attribute, or nothing if terminal is not initialized
 */
static int tfx_attributes(lua_State *L)
{
	maxargs(L, 2);
	if (!initialized) return 0;
	if (lua_gettop(L) > 0) {
		default_fg = (uint16_t) luaL_optinteger(L, 1, default_fg) & 0xFFFF;
		default_bg = (uint16_t) luaL_optinteger(L, 2, default_bg) & 0xFFFF;
		tb_set_clear_attributes(default_fg, default_bg);
	}
	lua_pushinteger(L, default_fg);
	lua_pushinteger(L, default_bg);
	return 2;
}

/* tfx_clear
 *
 * clears the terminal to the default foreground and background attributes.
 * If the optional foreground and background attribute arguments are
 * given, these will be set using tfx_bufSetAttributes before clearing.
 *
 * Arguments:
 *	L	Lua State
 *
 * Lua Stack:
 *	1	TfxBuffer userdata
 * 	2	(opt) foreground attributes, defaults to buffer default foreground
 * 	3	(opt) background attributes, defaults to buffer default background
 *
 * Lua Returns:
 *	-
 */
static int tfx_clear(lua_State *L)
{
	if (lua_gettop(L) != 0)
		tfx_attributes(L);
	if (initialized) tb_clear();
	return 0;
}

/* tfx_present
 *
 * update the terminal with the data from the back buffer
 *
 * Arguments:
 *	L	Lua State
 *
 * Lua Stack:
 *	-
 *
 * Lua Returns:
 *	-
 */
static int tfx_present(lua_State *L)
{
	maxargs(L, 0);
	if (initialized) tb_present();
	return 0;
}

/* tfx_setCursor
 *
 * set cursor position
 *
 * Arguments:
 *	L	Lua State
 *
 * Lua Stack:
 *	1	x coordinate
 * 	2	y coordinate
 *
 * Lua Returns:
 *	-
 */
static int tfx_setCursor(lua_State *L)
{
	maxargs(L, 2);
	int x = (int) luaL_checkinteger(L, 1) - top_left_coord;
	int y = (int) luaL_checkinteger(L, 2) - top_left_coord;
	if (initialized) tb_set_cursor(x, y);
	return 0;
}

/* tfx_hideCursor
 *
 * hide cursor
 *
 * Arguments:
 *	L	Lua State
 *
 * Lua Stack:
 *	-
 *
 * Lua Returns:
 *	-
 */
static int tfx_hideCursor(lua_State *L)
{
	maxargs(L, 0);
	if (initialized) tb_set_cursor(TB_HIDE_CURSOR, TB_HIDE_CURSOR);
	return 0;
}

/* tfx_setcell
 *
 * sets a cell on the terminal. Either uses the default values for foreground
 * and background, or the values passed on the lua stack.
 *
 * Arguments:
 *	L	Lua State
 *
 * Lua Stack:
 * 	1	x coordinate of cell
 * 	2	y coordinate of cell
 * either
 * 	3	TfxCell
 * or
 * 	3	(opt) char, defaults to ' '
 * 	4	(opt) foreground attributes, defaults to buffer default foreground
 * 	5	(opt) background attributes, defaults to buffer default background
 *
 * Lua Returns:
 *	-
 */
static int tfx_setCell(lua_State *L)
{
	int x = (int) luaL_checkinteger(L, 1) - top_left_coord;
	int y = (int) luaL_checkinteger(L, 2) - top_left_coord;
	if (tfx_isCell(L, 3)) {
		maxargs(L, 3);
		const TfxCell *tfxcell = tfx_checkCell(L, 3);
		if (initialized) tb_put_cell(x, y, tfxcell);
	} else {
		maxargs(L, 5);
		uint32_t ch = _tfx_optchar(L, 3, ' ');
		uint16_t fg = (uint16_t) luaL_optinteger(L, 4, default_fg) & 0xFFFF;
		uint16_t bg = (uint16_t) luaL_optinteger(L, 5, default_bg) & 0xFFFF;
		if (ch < ' ') ch = ' ';
		if (initialized) tb_change_cell(x, y, ch, fg, bg);
	}
	return 0;
}

/* tfx_getCell
 *
 * gets data from a cell on the terminal
 *
 * Arguments:
 *	L	Lua State
 *
 * Lua Stack:
 * 	1	x coordinate of cell
 * 	2	y coordinate of cell
 *
 * Lua Returns:
 *	a TfxCell with the data from the read cell
 */
int tfx_getCell(lua_State *L)
{
	int x = (int) luaL_checkinteger(L, 1) - top_left_coord;
	int y = (int) luaL_checkinteger(L, 2) - top_left_coord;

	if (x < 0 || x >= tb_width() || y < 0 || y >= tb_height())	{
		lua_pushnil(L);
		return 1;
	}
	TfxCell *tfxcell = tfx_pushCell(L);
	*tfxcell = CELL(tb_cell_buffer(), tb_width(), x, y);

	return 1;
}

/* tfx_blit
 *
 * blits the contents of a buffer to the terminal.
 *
 * Arguments:
 *	L	Lua State
 *
 * Lua Stack:
 * 	1	x coordinate of cell
 * 	2	y coordinate of cell
 * 	3	TfxBuffer
 *
 * Lua Returns:
 *	-
 */
static int tfx_blit(lua_State *L)
{
	maxargs(L, 3);
	int x = (int) luaL_checkinteger(L, 1) - top_left_coord;
	int y = (int) luaL_checkinteger(L, 2) - top_left_coord;
	TfxBuffer *tfxbuf = tfx_checkBuffer(L, 3);
	if (initialized) tbu_blit(x, y, tfxbuf->buf, tfxbuf->w, tfxbuf->h);
	return 0;
}

/* tfx_rect
 *
 * draws a rectangle. Either uses the default values for foreground and
 * background, or the values passed on the lua stack.
 * 
 * Arguments:
 *	L	Lua State
 *
 * Lua Stack:
 * 	1	x coordinate of rect
 * 	2	y coordinate of rect
 * 	3	width of rect
 * 	4	height of rect
 * either
 * 	5	TfxCell
 * or
 * 	5	(opt) char, defaults to ' '
 * 	6	(opt) foreground attributes, defaults to buffer default foreground
 * 	7	(opt) background attributes, defaults to buffer default background
 *
 * Lua Returns:
 *	false if the rectangle was entirely outside of the terminal, true if not
 */
static int tfx_rect(lua_State *L)
{
	if (!initialized) return 0;
	
	TfxCell cel, *celp = &cel;

	int x = (int) luaL_checkinteger(L, 1) - top_left_coord;
	int y = (int) luaL_checkinteger(L, 2) - top_left_coord;
	int w = (int) luaL_checkinteger(L, 3);
	int h = (int) luaL_checkinteger(L, 4);
	if (tfx_isCell(L, 5)) {
		maxargs(L, 5);
		celp = tfx_toCell(L, 5);
	} else {
		maxargs(L, 7);
		cel.ch = _tfx_optchar(L, 5, 0);
		cel.fg = (uint16_t) luaL_optinteger(L, 6, default_fg) & 0xFFFF;
		cel.bg = (uint16_t) luaL_optinteger(L, 7, default_bg) & 0xFFFF;
	}

	tbu_fillregion(x, y, w, h, celp);
	return 0;
}

/* tfx_copyRegion
 * 
 * copies a rectangular region to another place
 *
 * Arguments:
 *	L	Lua State
 *
 * Lua Stack
 * 	1	target x coordinate for copy
 * 	2	target y coordinate for copy
 * 	3	source x coordinate
 * 	4	source y coordinate
 *	5	width of rectangle to copy
 *	6	height of rectangle to copy
 *
 * Lua Returns:
 *	-
 */
int tfx_copyRegion(lua_State *L)
{
	if (!initialized) return 0;
	
	int tx = (int) luaL_checkinteger(L, 1) - top_left_coord;
	int ty = (int) luaL_checkinteger(L, 2) - top_left_coord;
	int x = (int) luaL_checkinteger(L, 3) - top_left_coord;
	int y = (int) luaL_checkinteger(L, 4) - top_left_coord;
	int w = (int) luaL_checkinteger(L, 5);
	int h = (int) luaL_checkinteger(L, 6);

	tbu_copyregion(tx, ty, x, y, w, h);
	return 0;
}

/* tfx_scrollRegion
 * 
 * scrolls a rectangular region
 *
 * Arguments:
 *	L	Lua State
 *
 * Lua Stack
 * 	1	x coordinate of rectangle to scroll
 * 	2	y coordinate of rectangle to scroll
 *	3	width of rectangle to scroll
 *	4	height of rectangle to scroll
 *	5	(opt) x scroll direction (-1, 0, 1), defaults to 0
 *	6	(opt) y scroll direction (-1, 0, 1), defaults to 0
 * either
 * 	7	TfxCell
 * or
 * 	7	(opt) char, defaults to ' '
 * 	8	(opt) foreground attributes, defaults to buffer default foreground
 * 	9	(opt) background attributes, defaults to buffer default background
 *
 * Lua Returns:
 *	-
 *
 * Note:
 *	arguments 5 and 6 (scroll directions) are ints, but only their sign is
 *	used. Scrolling is always by at most 1 cell.
 */
int tfx_scrollRegion(lua_State *L)
{
	if (!initialized) return 0;
	
	TfxCell cel, *celp = &cel;

	int x = (int) luaL_checkinteger(L, 1) - top_left_coord;
	int y = (int) luaL_checkinteger(L, 2) - top_left_coord;
	int w = (int) luaL_checkinteger(L, 3);
	int h = (int) luaL_checkinteger(L, 4);
	int sx = (int) luaL_optinteger(L, 5, 0);
	int sy = (int) luaL_optinteger(L, 6, 0);

	if (tfx_isCell(L, 7)) {
		maxargs(L, 7);
		celp = tfx_toCell(L, 7);
	} else {
		maxargs(L, 9);
		cel.ch = _tfx_optchar(L, 7, 0);
		cel.fg = (uint16_t) luaL_optinteger(L, 8, default_fg) & 0xFFFF;
		cel.bg = (uint16_t) luaL_optinteger(L, 9, default_bg) & 0xFFFF;
	}

	tbu_scrollregion(x, y, w, h, sx, sy, celp);
	return 0;
}

/* tfx_printAt
 *
 * prints some text to the top left position at x, y. Is used as both the
 * function termfx.printat and the TfxBuffer printat method, depending on
 * the first argument. 
 *
 * Arguments:
 *	L	Lua State
 *
 * Lua Stack:
 *  (1	TfxBuffer)
 *	+1	x coordinate of text
 * 	+2	y coordinate of text
 * 	+3	text (string or table of chars)
 * 	+4	(opt) maximum width to print
 *
 * Lua Returns:
 *	-
 */
static int tfx_printAt(lua_State *L)
{
	if (!initialized) return 0;
	
	TfxBuffer *tfxbuf = NULL;
	int fw, start = 1;
	uint16_t dfg = default_fg, dbg = default_bg;
	if (tfx_isBuffer(L, 1)) {
		tfxbuf = tfx_checkBuffer(L, 1);
		fw = tfxbuf->w;
		dfg = tfxbuf->fg;
		dbg = tfxbuf->bg;
		start = 2;
	} else {
		fw = tb_width();
	}
	
	int x = (int) luaL_checkinteger(L, start) - top_left_coord;
	int y = (int) luaL_checkinteger(L, start + 1) - top_left_coord;
	int pw = -1;
	if (lua_gettop(L) == start + 3)
		pw = (int) luaL_checkinteger(L, start + 3);
	
	if (lua_type(L, start + 2) == LUA_TTABLE) {
		int i = 1;
		int w = 0;
		if (pw < 0) {
			pw = lua_rawlen(L, start + 2);
		}
		lua_rawgeti(L, start + 2, i);
		while (!lua_isnil(L, -1) && w < pw && x + w < fw) {
			TfxCell *c = tfx_toCell(L, -1);
			if (c) {
				if (tfxbuf)
					_tfx_bufChangeCell(tfxbuf, x + w, y, c);
				else
					tb_put_cell(x + w, y, c);
			}
			++w;
			++i;
			lua_rawgeti(L, start + 2, i);
		}
	} else if (!lua_isnil(L, start + 2)) {
		const char* str = luaL_checkstring(L, start + 2);
		int isutf8 = mini_utf8_check_encoding(str) == 0;
		int w = 0;
		if (pw < 0) {
			pw = isutf8 ? mini_utf8_strlen(str) : strlen(str);
		}
		struct tb_cell c;
		c.fg = dfg;
		c.bg = dbg;
		c.ch = isutf8 ? mini_utf8_decode(&str) : *str++;
		if (c.ch < ' ' && c.ch > 0) c.ch = ' ';
		
		for (w = 0; c.ch && (w < pw) && (x + w < fw); ++w) {
			if (tfxbuf)
				_tfx_bufChangeCell(tfxbuf, x + w, y, &c);
			else
				tb_put_cell(x + w, y, &c);
			c.ch = isutf8 ? mini_utf8_decode(&str) : *str++;
			if (c.ch < ' ' && c.ch > 0) c.ch = ' ';
		}
	}
	return 0;
}

/* tfx_inputMode
 *
 * set or query input mode
 *
 * Arguments:
 *	L	Lua State
 *
 * Lua Stack:
 *	1	input mode, or nil to query
 *
 * Lua Returns:
 *	+1	current input mode
 */
static int tfx_inputMode(lua_State *L)
{
	maxargs(L, 1);
	int mode = TB_INPUT_CURRENT;
	if (!lua_isnoneornil(L, 1))
		mode = (int) luaL_checkinteger(L, 1);
	int res = tb_select_input_mode(mode);
	lua_pushinteger(L, res);
	return 1;
}

/* tfx_outputMode
 *
 * set or query output mode
 *
 * Arguments:
 *	L	Lua State
 *
 * Lua Stack:
 *	1	output mode, or nil to query
 *
 * Lua Returns:
 *	+1	current output mode
 */
static int tfx_outputMode(lua_State *L)
{
	maxargs(L, 1);
	int mode = TB_OUTPUT_CURRENT;
	if (!lua_isnoneornil(L, 1))
		mode = (int) luaL_checkinteger(L, 1);
	int res = tb_select_output_mode(mode);
	lua_pushinteger(L, res);
	return 1;
}

/* tfx_pollEvent
 *
 * polls next event.
 *
 * Arguments:
 *	L	Lua State
 *
 * Lua Stack:
 *	1	(opt) timeout in ms, waits forever for the next event if not set
 *
 * Lua Returns:
 *	+1	a table with the event data
 */
static int tfx_pollEvent(lua_State *L)
{
	maxargs(L, 1);
	struct tb_event evt;
	int eno = 0;
	uint32_t started = _tfx_getMsTimer();
	if (lua_gettop(L) == 1 && !lua_isnil(L, 1)) {
		uint32_t timeout = (int) luaL_checkinteger(L, 1);
		eno = initialized ? tb_peek_event(&evt, timeout) : 0;
	} else
		eno = initialized ? tb_poll_event(&evt) : 0;

	if (eno > 0) {
		lua_newtable(L);
		
		lua_pushliteral(L, "type");
		switch (evt.type) {
			case TB_EVENT_KEY: lua_pushliteral(L, "key"); break;
			case TB_EVENT_RESIZE: lua_pushliteral(L, "resize"); break;
#ifdef TB_EVENT_MOUSE
			case TB_EVENT_MOUSE: lua_pushliteral(L, "mouse"); break;
#endif
			default: lua_pushliteral(L, "unknown");
		}
		lua_rawset(L, -3);
		
		lua_pushliteral(L, "elapsed");
		lua_pushinteger(L, _tfx_getMsTimer() - started);
		lua_rawset(L, -3);
		
		if (evt.type == TB_EVENT_KEY) {
			lua_pushliteral(L, "mod");
			switch (evt.mod) {
				case TB_MOD_ALT: lua_pushliteral(L, "ALT"); break;
				default: lua_pushnil(L);
			}
			lua_rawset(L, -3);
			
			lua_pushliteral(L, "key");
			if (evt.ch == 0) {
				lua_pushinteger(L, evt.key);
			} else {
				lua_pushnil(L);
			}
			lua_rawset(L, -3);
			
			lua_pushliteral(L, "ch");
			if (evt.ch != 0) {
				lua_pushinteger(L, evt.ch);
			} else {
				lua_pushnil(L);
			}
			lua_rawset(L, -3);
			
			lua_pushliteral(L, "char");
			char c[10];
			memset(c, 0, 10);
			if (evt.ch < 0x7F)
				c[0] = evt.ch;
			else
				mini_utf8_encode(evt.ch, c, 10);
			lua_pushstring(L, c);
			lua_rawset(L, -3);
			
		} else if (evt.type == TB_EVENT_RESIZE) {
			lua_pushliteral(L, "w");
			lua_pushinteger(L, evt.w);
			lua_rawset(L, -3);

			lua_pushliteral(L, "h");
			lua_pushinteger(L, evt.h);
			lua_rawset(L, -3);
#ifdef TB_EVENT_MOUSE
		} else if (evt.type == TB_EVENT_MOUSE) {
			lua_pushliteral(L, "x");
			lua_pushinteger(L, evt.x + top_left_coord);
			lua_rawset(L, -3);

			lua_pushliteral(L, "y");
			lua_pushinteger(L, evt.y + top_left_coord);
			lua_rawset(L, -3);

			lua_pushliteral(L, "key");
			lua_pushinteger(L, evt.key);
			lua_rawset(L, -3);
#endif
		}
	} else if (eno == 0) {
		lua_pushnil(L);
	} else {
		lua_pushnil(L);
		lua_pushstring(L, strerror(errno));
		return 2;
	}
	return 1;
}

/* methods for the TfxBuffer userdata
 */
static const struct luaL_Reg ltfxbuf_methods [] = {
	{"attributes", tfx_bufAttributes},
	{"clear", tfx_bufClear},
	{"setcell", tfx_bufSetcell},
	{"getcell", tfx_bufGetCell},
	{"blit", tfx_bufBlit},
	{"rect", tfx_bufRect},
	{"copyregion", tfx_bufCopyRegion},
	{"scrollregion", tfx_bufScrollRegion},
	{"printat", tfx_printAt},
	{"width", tfx_bufWidth},
	{"height", tfx_bufHeight},
	{"size", tfx_bufSize},
	
	{NULL, NULL}
};

/* TermFX function list
 */
static const struct luaL_Reg ltermfx [] ={
	{"init", tfx_init},
	{"shutdown", tfx_shutdown},
	{"width", tfx_width},
	{"height", tfx_height},
	{"size", tfx_size},
	{"clear", tfx_clear},
	{"attributes", tfx_attributes},
	{"present", tfx_present},
	{"setcursor", tfx_setCursor},
	{"hidecursor", tfx_hideCursor},
	{"newcell", tfx_newCell},
	{"setcell", tfx_setCell},
	{"getcell", tfx_getCell},
	{"newbuffer", tfx_newBuffer},
	{"blit", tfx_blit},
	{"rect", tfx_rect},
	{"copyregion", tfx_copyRegion},
	{"scrollregion", tfx_scrollRegion},
	{"printat", tfx_printAt},
	{"inputmode", tfx_inputMode},
	{"outputmode", tfx_outputMode},
	{"pollevent", tfx_pollEvent},
	
	{NULL, NULL}
};


/* helper: __index metamethod for the table containing the color values
 * for the predefined color names. As the values for these names are not
 * constant across the output modes, some tweaking is needed.
 */
static int tfx__indexColor(lua_State *L)
{
	const char* what = luaL_checkstring(L, 2);
	int omode = tb_select_output_mode(TB_OUTPUT_CURRENT);
	int col = -1;

	if (!strcmp("DEFAULT", what))  col = TB_DEFAULT;
	else if (!strcmp("BLACK", what)) col = TB_BLACK;
	else if (!strcmp("RED", what)) col = TB_RED;
	else if (!strcmp("GREEN", what)) col = TB_GREEN;
	else if (!strcmp("YELLOW", what)) col = TB_YELLOW;
	else if (!strcmp("BLUE", what)) col = TB_BLUE;
	else if (!strcmp("MAGENTA", what)) col = TB_MAGENTA;
	else if (!strcmp("CYAN", what)) col = TB_CYAN;
	else if (!strcmp("WHITE", what)) col = TB_WHITE;
	
	switch (omode) {
		case TB_OUTPUT_256:
			col -= 1;
			break;
		case TB_OUTPUT_GRAYSCALE:
			col = (col - 1) * 3;
			if (col > 6) col += 1;
			if (col > 15) col += 1;
			break;
		case TB_OUTPUT_216:
			switch (col) {
				case TB_BLACK: col = 0; break;
				case TB_RED: col = 72; break;
				case TB_GREEN: col = 12; break;
				case TB_YELLOW: col = 114; break;
				case TB_BLUE: col = 2; break;
				case TB_MAGENTA: col = 74; break;
				case TB_CYAN: col = 14; break;
				case TB_WHITE: col = 129; break;
				default:;
			}
			break;
		default:;
	}
	
	if (col < 0)
		lua_pushnil(L);
	else
		lua_pushinteger(L, col);

	return 1;
}

/* add constants */
#define ADDCONST(n, v) \
	lua_pushliteral(L, n); \
	lua_pushinteger(L, v); \
	lua_rawset(L, -3)

static void tfx_addconstants(lua_State *L)
{
	lua_pushliteral(L, "key");
	lua_newtable(L);
	ADDCONST("F1", TB_KEY_F1);
	ADDCONST("F2", TB_KEY_F2);
	ADDCONST("F3", TB_KEY_F3);
	ADDCONST("F4", TB_KEY_F4);
	ADDCONST("F5", TB_KEY_F5);
	ADDCONST("F6", TB_KEY_F6);
	ADDCONST("F7", TB_KEY_F7);
	ADDCONST("F8", TB_KEY_F8);
	ADDCONST("F9", TB_KEY_F9);
	ADDCONST("F10", TB_KEY_F10);
	ADDCONST("F11", TB_KEY_F11);
	ADDCONST("F12", TB_KEY_F12);
	ADDCONST("INSERT", TB_KEY_INSERT);
	ADDCONST("DELETE", TB_KEY_DELETE);
	ADDCONST("HOME", TB_KEY_HOME);
	ADDCONST("END", TB_KEY_END);
	ADDCONST("PGUP", TB_KEY_PGUP);
	ADDCONST("PGDN", TB_KEY_PGDN);
	ADDCONST("ARROW_UP", TB_KEY_ARROW_UP);
	ADDCONST("ARROW_DOWN", TB_KEY_ARROW_DOWN);
	ADDCONST("ARROW_LEFT", TB_KEY_ARROW_LEFT);
	ADDCONST("ARROW_RIGHT", TB_KEY_ARROW_RIGHT);

#ifdef TB_INPUT_MOUSE
	ADDCONST("MOUSE_LEFT", TB_KEY_MOUSE_LEFT);
	ADDCONST("MOUSE_RIGHT", TB_KEY_MOUSE_RIGHT);
	ADDCONST("MOUSE_MIDDLE", TB_KEY_MOUSE_MIDDLE);
	ADDCONST("MOUSE_RELEASE", TB_KEY_MOUSE_RELEASE);
	ADDCONST("MOUSE_WHEEL_UP", TB_KEY_MOUSE_WHEEL_UP);
	ADDCONST("MOUSE_WHEEL_DOWN", TB_KEY_MOUSE_WHEEL_DOWN);
#endif

	ADDCONST("CTRL_TILDE", TB_KEY_CTRL_TILDE);
	ADDCONST("CTRL_2", TB_KEY_CTRL_2);
	ADDCONST("CTRL_A", TB_KEY_CTRL_A);
	ADDCONST("CTRL_B", TB_KEY_CTRL_B);
	ADDCONST("CTRL_C", TB_KEY_CTRL_C);
	ADDCONST("CTRL_D", TB_KEY_CTRL_D);
	ADDCONST("CTRL_E", TB_KEY_CTRL_E);
	ADDCONST("CTRL_F", TB_KEY_CTRL_F);
	ADDCONST("CTRL_G", TB_KEY_CTRL_G);
	ADDCONST("BACKSPACE", TB_KEY_BACKSPACE);
	ADDCONST("CTRL_H", TB_KEY_CTRL_H);
	ADDCONST("TAB", TB_KEY_TAB);
	ADDCONST("CTRL_I", TB_KEY_CTRL_I);
	ADDCONST("CTRL_J", TB_KEY_CTRL_J);
	ADDCONST("CTRL_K", TB_KEY_CTRL_K);
	ADDCONST("CTRL_L", TB_KEY_CTRL_L);
	ADDCONST("ENTER", TB_KEY_ENTER);
	ADDCONST("CTRL_M", TB_KEY_CTRL_M);
	ADDCONST("CTRL_N", TB_KEY_CTRL_N);
	ADDCONST("CTRL_O", TB_KEY_CTRL_O);
	ADDCONST("CTRL_P", TB_KEY_CTRL_P);
	ADDCONST("CTRL_Q", TB_KEY_CTRL_Q);
	ADDCONST("CTRL_R", TB_KEY_CTRL_R);
	ADDCONST("CTRL_S", TB_KEY_CTRL_S);
	ADDCONST("CTRL_T", TB_KEY_CTRL_T);
	ADDCONST("CTRL_U", TB_KEY_CTRL_U);
	ADDCONST("CTRL_V", TB_KEY_CTRL_V);
	ADDCONST("CTRL_W", TB_KEY_CTRL_W);
	ADDCONST("CTRL_X", TB_KEY_CTRL_X);
	ADDCONST("CTRL_Y", TB_KEY_CTRL_Y);
	ADDCONST("CTRL_Z", TB_KEY_CTRL_Z);
	ADDCONST("ESC", TB_KEY_ESC);
	ADDCONST("CTRL_LSQ_BRACKET", TB_KEY_CTRL_LSQ_BRACKET);
	ADDCONST("CTRL_3", TB_KEY_CTRL_3);
	ADDCONST("CTRL_4", TB_KEY_CTRL_4);
	ADDCONST("CTRL_BACKSLASH", TB_KEY_CTRL_BACKSLASH);
	ADDCONST("CTRL_5", TB_KEY_CTRL_5);
	ADDCONST("CTRL_RSQ_BRACKET", TB_KEY_CTRL_RSQ_BRACKET);
	ADDCONST("CTRL_6", TB_KEY_CTRL_6);
	ADDCONST("CTRL_7", TB_KEY_CTRL_7);
	ADDCONST("CTRL_SLASH", TB_KEY_CTRL_SLASH);
	ADDCONST("CTRL_UNDERSCORE", TB_KEY_CTRL_UNDERSCORE);
	ADDCONST("SPACE", TB_KEY_SPACE);
	ADDCONST("BACKSPACE2", TB_KEY_BACKSPACE2);
	ADDCONST("CTRL_8", TB_KEY_CTRL_8);
	lua_rawset(L, -3);

	lua_pushliteral(L, "color");
	lua_newtable(L);
	lua_newtable(L);
	lua_pushliteral(L, "__index");
	lua_pushcfunction(L, tfx__indexColor);
	lua_rawset(L, -3);
	lua_setmetatable(L, -2);
	lua_rawset(L, -3);

	lua_pushliteral(L, "format");
	lua_newtable(L);
	ADDCONST("BOLD", TB_BOLD);
	ADDCONST("UNDERLINE", TB_UNDERLINE);
	ADDCONST("REVERSE", TB_REVERSE);
	lua_rawset(L, -3);

	lua_pushliteral(L, "input");
	lua_newtable(L);
	ADDCONST("ESC", TB_INPUT_ESC);
	ADDCONST("ALT", TB_INPUT_ALT);
#ifdef TB_INPUT_MOUSE
	ADDCONST("MOUSE", TB_INPUT_MOUSE);
#endif
	lua_rawset(L, -3);

	lua_pushliteral(L, "output");
	lua_newtable(L);
	ADDCONST("NORMAL", TB_OUTPUT_NORMAL);
	ADDCONST("COL256", TB_OUTPUT_256);
	ADDCONST("COL216", TB_OUTPUT_216);
	ADDCONST("GRAYSCALE", TB_OUTPUT_GRAYSCALE);
	ADDCONST("GREYSCALE", TB_OUTPUT_GRAYSCALE);
	lua_rawset(L, -3);
}

/* luaopen_ltermbox
 * 
 * open and initialize this library
 */
int luaopen_termfx(lua_State *L)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	mstimer_tm0 = tv.tv_sec * 1000 + tv.tv_usec / 1000;

	luaL_newlib(L, ltermfx);
	tfx_color_init(L);
	tfx_addconstants(L);

	lua_pushliteral(L, "_VERSION");
	lua_pushliteral(L, _VERSION);
	lua_rawset(L, -3);

	luaL_newmetatable(L, TFXCELL);
	luaL_setfuncs(L, tfx_CellMeta, 0);
	lua_pop(L, 1);

	luaL_newmetatable(L, TFXBUFFER);
	luaL_setfuncs(L, tfx_BufferMeta, 0);
	lua_pushliteral(L, "__index");
	luaL_newlib(L, ltfxbuf_methods);
	lua_rawset(L, -3);
	lua_pop(L, 1);

	/* shutdown termbox or suffer the consequences */
	atexit(_tfx_doShutdown);

	return 1;
}
