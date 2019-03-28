include ./platform.mk

LUA_INC ?= lua/src

LUA_CLIB_PATH ?= luaclib/

TERMFX_INC ?= lualib-src/termfx
LSOCKET_INC ?= lualib-src/lsocket

TLS_LIB=/usr/local/ssl/lib
TLS_INC=/usr/local/ssl/include

TERMFX_SO_NAME = termfx.so
LSOCKET_NAME = lsocket.so
CRYPT_SO 		 		= $(LUA_CLIB_PATH)crypt.so
TLS_SO                  = $(LUA_CLIB_PATH)ltls.so
LSOCKET_SO  			= $(LUA_CLIB_PATH)$(LSOCKET_NAME)
TERMFX_SO  			= $(LUA_CLIB_PATH)$(TERMFX_SO_NAME)

all: luabin $(TLS_SO) $(TERMFX_SO) \
	$(LSOCKET_SO) $(CRYPT_SO)  \

#####################################################

ifeq ($(PLAT),macosx)
	OS = Darwin
else
	OS = linux
endif

luabin:
	cd lua && $(MAKE) PLAT=$(PLAT)

$(TERMFX_SO) : | $(LUA_CLIB_PATH)
	cd $(TERMFX_INC) && $(MAKE) OS=$(OS) LUAROOT=../../$(LUA_INC)
	cp -f $(TERMFX_INC)/$(TERMFX_SO_NAME) $@

$(CRYPT_SO) : lualib-src/crypt/lua-crypt.c lualib-src/crypt/lsha1.c | $(LUA_CLIB_PATH)
	$(CC) $(CFLAGS) $(SHARED) $^ -o $@ -I$(LUA_INC)

$(TLS_SO) : lualib-src/crypt/ltls.c | $(LUA_CLIB_PATH)
	$(CC) $(CFLAGS) $(SHARED) -L$(TLS_LIB) -I$(TLS_INC) -I$(LUA_INC) $^ -o $@ -lssl

$(LSOCKET_SO) : $(LUA_CLIB_PATH)
	cd $(LSOCKET_INC) && $(MAKE) LUA_INCDIR=../../$(LUA_INC)
	cp -f $(LSOCKET_INC)/$(LSOCKET_NAME) $@


#####################################################

client:
	./lua/src/lua ./client.lua

clean:
	rm -f $(LUA_CLIB_PATH)*.so
	cd $(LUA_INC) && $(MAKE) clean

.PHONY: all clean
