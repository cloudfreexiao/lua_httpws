/* termfx_color.c
 *
 * provide simple terminal interface for lua
 *
 * Gunnar Zötl <gz@tset.de>, 2014-2015
 * Released under the terms of the MIT license. See file LICENSE for details.
 */

#include "lua.h"
#include "lauxlib.h"

#include "termbox.h"
#include "termfx.h"

static const char* xterm_color_data[256] = {
	"#000000", "#800000", "#008000", "#808000", "#000080", "#800080", "#008080", "#c0c0c0",
	"#808080", "#ff0000", "#00ff00", "#ffff00", "#0000ff", "#ff00ff", "#00ffff", "#ffffff",
	
	"#000000", "#00005f", "#000087", "#0000af", "#0000d7", "#0000ff",
	"#005f00", "#005f5f", "#005f87", "#005faf", "#005fd7", "#005fff",
	"#008700", "#00875f", "#008787", "#0087af", "#0087d7", "#0087ff",
	"#00af00", "#00af5f", "#00af87", "#00afaf", "#00afd7", "#00afff",
	"#00d700", "#00d75f", "#00d787", "#00d7af", "#00d7d7", "#00d7ff",
	"#00ff00", "#00ff5f", "#00ff87", "#00ffaf", "#00ffd7", "#00ffff",
	"#5f0000", "#5f005f", "#5f0087", "#5f00af", "#5f00d7", "#5f00ff",
	"#5f5f00", "#5f5f5f", "#5f5f87", "#5f5faf", "#5f5fd7", "#5f5fff",
	"#5f8700", "#5f875f", "#5f8787", "#5f87af", "#5f87d7", "#5f87ff",
	"#5faf00", "#5faf5f", "#5faf87", "#5fafaf", "#5fafd7", "#5fafff",
	"#5fd700", "#5fd75f", "#5fd787", "#5fd7af", "#5fd7d7", "#5fd7ff",
	"#5fff00", "#5fff5f", "#5fff87", "#5fffaf", "#5fffd7", "#5fffff",
	"#870000", "#87005f", "#870087", "#8700af", "#8700d7", "#8700ff",
	"#875f00", "#875f5f", "#875f87", "#875faf", "#875fd7", "#875fff",
	"#878700", "#87875f", "#878787", "#8787af", "#8787d7", "#8787ff",
	"#87af00", "#87af5f", "#87af87", "#87afaf", "#87afd7", "#87afff",
	"#87d700", "#87d75f", "#87d787", "#87d7af", "#87d7d7", "#87d7ff",
	"#87ff00", "#87ff5f", "#87ff87", "#87ffaf", "#87ffd7", "#87ffff",
	"#af0000", "#af005f", "#af0087", "#af00af", "#af00d7", "#af00ff",
	"#af5f00", "#af5f5f", "#af5f87", "#af5faf", "#af5fd7", "#af5fff",
	"#af8700", "#af875f", "#af8787", "#af87af", "#af87d7", "#af87ff",
	"#afaf00", "#afaf5f", "#afaf87", "#afafaf", "#afafd7", "#afafff",
	"#afd700", "#afd75f", "#afd787", "#afd7af", "#afd7d7", "#afd7ff",
	"#afff00", "#afff5f", "#afff87", "#afffaf", "#afffd7", "#afffff",
	"#d70000", "#d7005f", "#d70087", "#d700af", "#d700d7", "#d700ff",
	"#d75f00", "#d75f5f", "#d75f87", "#d75faf", "#d75fd7", "#d75fff",
	"#d78700", "#d7875f", "#d78787", "#d787af", "#d787d7", "#d787ff",
	"#d7af00", "#d7af5f", "#d7af87", "#d7afaf", "#d7afd7", "#d7afff",
	"#d7d700", "#d7d75f", "#d7d787", "#d7d7af", "#d7d7d7", "#d7d7ff",
	"#d7ff00", "#d7ff5f", "#d7ff87", "#d7ffaf", "#d7ffd7", "#d7ffff",
	"#ff0000", "#ff005f", "#ff0087", "#ff00af", "#ff00d7", "#ff00ff",
	"#ff5f00", "#ff5f5f", "#ff5f87", "#ff5faf", "#ff5fd7", "#ff5fff",
	"#ff8700", "#ff875f", "#ff8787", "#ff87af", "#ff87d7", "#ff87ff",
	"#ffaf00", "#ffaf5f", "#ffaf87", "#ffafaf", "#ffafd7", "#ffafff",
	"#ffd700", "#ffd75f", "#ffd787", "#ffd7af", "#ffd7d7", "#ffd7ff",
	"#ffff00", "#ffff5f", "#ffff87", "#ffffaf", "#ffffd7", "#ffffff",
	
	"#080808", "#121212", "#1c1c1c", "#262626", "#303030", "#3a3a3a",
	"#444444", "#4e4e4e", "#585858", "#626262", "#6c6c6c", "#767676",
	"#808080", "#8a8a8a", "#949494", "#9e9e9e", "#a8a8a8", "#b2b2b2",
	"#bcbcbc", "#c6c6c6", "#d0d0d0", "#dadada", "#e4e4e4", "#eeeeee"
};

/* tfx_rgb2color
 *
 * maps r, g, b values in the range 0..5 to an xterm color number. Works
 * only in COL216 and COL256 modes. As can be seen by the table above,
 * the 216 colors are arranged such that the color for any rgb triplet
 * can be found at 16 + (r * 6 + g) * 6 + b, for 0 <= r, g, b <= 5
 *
 * Arguments:
 * 	L	Lua State
 *
 * Lua Stack:
 *	1	red value
 * 	2	green value
 * 	3	blue value
 *
 * Lua Returns:
 *	+1	the color number, or nil on error.
 */
static int tfx_rgb2color(lua_State *L)
{
	maxargs(L, 3);
	unsigned int r = luaL_checkinteger(L, 1);
	unsigned int g = luaL_checkinteger(L, 2);
	unsigned int b = luaL_checkinteger(L, 3);
	
	if (r < 0 || r > 5 || g < 0 || g > 5 || b < 0 || b > 5) {
		lua_pushnil(L);
		return 1;
	}

	int omode = tb_select_output_mode(TB_OUTPUT_CURRENT);
	if (omode == TB_OUTPUT_256 || omode == TB_OUTPUT_216) {
		int col = (r * 6 + g) * 6 + b;
		if (omode == TB_OUTPUT_256) col += 16;
		lua_pushinteger(L, col);
	} else {
		lua_pushnil(L);
	}

	return 1;
}

/* tfx_rgb2color
 *
 * maps a grey value in the range 0..25 to an xterm color number. Works
 * only in GREYSCALE and COL256 modes. The greys are in one consecutive
 * block, starting from 232, except for #000000 and #ffffff, which are
 * only available in COL256 mode.
 *
 * Arguments:
 * 	L	Lua State
 *
 * Lua Stack:
 *	1	grey value
 *
 * Lua Returns:
 *	+1	the color number, or nil on error.
 */
static int tfx_grey2color(lua_State *L)
{
	maxargs(L, 1);
	unsigned int v = luaL_checkinteger(L, 1);
	if (v < 0 || v > 25) {
		lua_pushnil(L);
		return 1;
	}

	int omode = tb_select_output_mode(TB_OUTPUT_CURRENT);
	if (omode == TB_OUTPUT_256 || omode == TB_OUTPUT_GRAYSCALE) {
		int col;
		if (omode == TB_OUTPUT_GRAYSCALE) {
			if (v < 1)
				v = 1;
			else if (v > 24)
				v = 24;
		}
		if (v == 0)
			col = 16;
		else if (v == 25)
			col = 231;
		else
			col = 231 + v;
		if (omode == TB_OUTPUT_GRAYSCALE)
			col -= 232;
		lua_pushinteger(L, col);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

/* tfx_colorinfo
 *
 * finds the color string from a xterm color number from the above table,
 * and also returns its r, g, b values.
 *
 * Arguments:
 * 	L	Lua State
 *
 * Lua Stack:
 *	1	color number
 *
 * Lua Returns:
 *	+1	color string "#XXXXXX", or nil on error.
 * 	+2	r value or nothing
 * 	+3	g value or nothing
 * 	+4	b value or nothing
 */
static int tfx_colorinfo(lua_State *L)
{
	int omode = tb_select_output_mode(TB_OUTPUT_CURRENT);
	unsigned int col = luaL_checkinteger(L, 1);
	if ((omode == TB_OUTPUT_NORMAL && col >= 16) || col < 0 || col > 255) {
		lua_pushnil(L);
		lua_pushnil(L);
		return 2;
	}
	maxargs(L, 1);
	if (omode == TB_OUTPUT_NORMAL) {
		if (col > 0 && col <= 8) {
			col -= 1;
		} else {
			col = 8;
		}
	} else if (omode == TB_OUTPUT_216) {
		col += 16;
	} else if (omode == TB_OUTPUT_GRAYSCALE) {
		col += 232;
	}
	if (col < 256) {
		unsigned int r, g, b;
		lua_pushstring(L, xterm_color_data[col]);
		sscanf(xterm_color_data[col], "#%02x%02x%02x", &r, &g, &b);
		lua_pushinteger(L, r);
		lua_pushinteger(L, g);
		lua_pushinteger(L, b);
		return 4;
	}
	lua_pushnil(L);
	return 1;
}

/* TermFX color handling function list
 */
static const struct luaL_Reg ltermfx_color [] ={
	{"rgb2color", tfx_rgb2color},
	{"grey2color", tfx_grey2color},
	{"colorinfo", tfx_colorinfo},
	
	{NULL, NULL}
};

/* export color functions into termfx function table
 */
void tfx_color_init(lua_State *L)
{
	luaL_setfuncs(L, ltermfx_color, 0);
}
