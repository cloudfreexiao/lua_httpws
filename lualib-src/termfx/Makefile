# simple Makefile for termfx. Works for Linux, MacOS X, probably other unixen
#
# Gunnar Zötl <gz@tset.de>, 2014-2015.
# Released under the terms of the MIT license. See file LICENSE for details.

TERMBOX = ./termbox

# try some automatic discovery. If that does not work for you, just set
# the following values manually.
OS = $(shell uname -s)
LUAVERSION = $(shell lua -e "print(string.match(_VERSION, '%d+%.%d+'))")
LUA_BINDIR = $(shell dirname `which lua`)
LUAROOT ?= $(shell dirname $(LUA_BINDIR))

OBJS = termfx.o termfx_color.o tbutils.o

TARGET = termfx.so

CC = gcc
CFLAGS = -fPIC -Wall
LUA_INCDIR ?= $(LUAROOT)
LUA_LIBDIR ?= $(LUAROOT)

# OS specialities
ifeq ($(OS),Darwin)
LIBFLAG = -bundle -undefined dynamic_lookup -all_load
else
LIBFLAG = -shared
endif

ifdef DEBUG
CCFLAGS=-g $(CFLAGS)
CLDFLAGS=-g -lefence $(LIBFLAG)
WAFFLAGS=--debug
else
CCFLAGS=$(CFLAGS)
CLDFLAGS=$(LIBFLAG)
endif

# install target locations
INST_DIR = /usr/local
INST_LIBDIR = $(INST_DIR)/lib/lua/$(LUAVERSION)
INST_LUADIR = $(INST_DIR)/share/lua/$(LUAVERSION)

all: $(TARGET)

$(TARGET): $(OBJS) libtermbox.a
	$(CC) $(CLDFLAGS) -o $@ -L$(LUA_LIBDIR) $(OBJS) -L. -ltermbox

%.o: %.c termbox.h termfx.h
	$(CC) $(CCFLAGS) -I$(LUA_INCDIR) -c $< -o $@

$(TERMBOX):
	git clone https://github.com/nsf/termbox.git

termbox.h: $(TERMBOX)/src/termbox.h
	cp $(TERMBOX)/src/$@ .

libtermbox.a: $(TERMBOX)/build/src/libtermbox.a
	cp $< $@

$(TERMBOX)/build/src/libtermbox.a: $(TERMBOX)
	(cd $(TERMBOX); \
	unset CC; \
	env CFLAGS="-fPIC" ./waf configure $(WAFFLAGS); \
	./waf build --targets=termbox_static -v )

$(TERMBOX)/src/termbox.h: $(TERMBOX)
	touch $@

install:
	mkdir -p $(INST_LIBDIR)
	cp $(TARGET) $(INST_LIBDIR)

clean:
	find . -name "*~" -exec rm {} \;
	find . -name .DS_Store -exec rm {} \;
	find . -name "._*" -exec rm {} \;
	rm -f *.a *.o *.so core termbox.h
	rm -f screenshot.html samples/screenshot.html
	(cd $(TERMBOX) && ./waf distclean)

distclean: clean
	rm -rf $(TERMBOX)

check:
	cppcheck *.c

dist: $(TERMBOX) clean
	cd $(TERMBOX) && rm -rf .git .gitignore
