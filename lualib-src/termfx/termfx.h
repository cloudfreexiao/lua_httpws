/* termfx.h
 *
 * provide simple terminal interface for lua
 *
 * Gunnar Zötl <gz@tset.de>, 2014-2015
 * Released under the terms of the MIT license. See file LICENSE for details.
 */

#define _VERSION "0.7.1"

#define TFXCELL "TfxCell"
#define TFXBUFFER "TfxBuffer"
#define TOSTRING_BUFSIZ 64

#if LUA_VERSION_NUM == 501
#define luaL_newlib(L,funcs) lua_newtable(L); luaL_register(L, NULL, funcs)
#define luaL_setfuncs(L,funcs,x) luaL_register(L, NULL, funcs)
#define lua_rawlen(L, i) lua_objlen(L, i)
#endif

#define maxargs(L, n) if (lua_gettop(L) > (n)) { return luaL_error(L, "invalid number of arguments."); }

/* from termfx_color.c */
extern void tfx_color_init(lua_State *L);

