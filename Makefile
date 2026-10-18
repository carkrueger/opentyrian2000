# BUILD SETTINGS ###############################################################

# `emmake make` sets CC to emcc for a WebAssembly build.
ifneq ($(findstring emcc, $(notdir $(CC))), )
    PLATFORM := EMSCRIPTEN
    TYRIAN_DIR = /data
else ifneq ($(filter Msys Cygwin, $(shell uname -o)), )
    PLATFORM := WIN32
    TYRIAN_DIR = C:\\TYRIAN
else
    PLATFORM := UNIX
    TYRIAN_DIR = $(gamesdir)/opentyrian2000
endif

# true, false, or auto (true if pkg-config can find SDL2_net)
WITH_NETWORK := auto

# Emscripten: directory with the game data to bundle (see get_data.sh)
DATA_DIR ?= data

################################################################################

# see https://www.gnu.org/prep/standards/html_node/Makefile-Conventions.html

SHELL = /bin/sh

CC ?= gcc
INSTALL ?= install
PKG_CONFIG ?= pkg-config
WINDRES ?= windres

VCS_IDREV ?= (git describe --tags || git rev-parse --short HEAD)

INSTALL_PROGRAM ?= $(INSTALL)
INSTALL_DATA ?= $(INSTALL) -m 644

prefix ?= /usr/local
exec_prefix ?= $(prefix)

bindir ?= $(exec_prefix)/bin
datarootdir ?= $(prefix)/share
datadir ?= $(datarootdir)
docdir ?= $(datarootdir)/doc/opentyrian2000
mandir ?= $(datarootdir)/man
man6dir ?= $(mandir)/man6
man6ext ?= .6
desktopdir ?= $(datarootdir)/applications
icondir ?= $(datarootdir)/icons

# see https://www.pathname.com/fhs/pub/fhs-2.3.html

gamesdir ?= $(datadir)/games

###

TARGET := opentyrian2000
RES :=
ifeq ($(PLATFORM), WIN32)
    TARGET := opentyrian2000.exe
    # The icon, from the same resource script the Visual Studio build uses.
    RES := obj/resources.o
endif
ifeq ($(PLATFORM), EMSCRIPTEN)
    # Also produces opentyrian2000.js, .wasm, and .data (the bundled game data).
    TARGET := opentyrian2000.html
    WEB_FILES := web/shell.html web/pre.js web/library.js
    # SDL2_net is not available in the browser.
    WITH_NETWORK := false
endif

SRCS := $(wildcard src/*.c)
OBJS := $(SRCS:src/%.c=obj/%.o)
DEPS := $(SRCS:src/%.c=obj/%.d)

###

ifeq ($(WITH_NETWORK), auto)
    ifeq ($(shell $(PKG_CONFIG) --exists SDL2_net 2>/dev/null && echo true), true)
        WITH_NETWORK := true
    else
        WITH_NETWORK := false
        $(info SDL2_net not found; building without network support)
    endif
endif

ifeq ($(WITH_NETWORK), true)
    EXTRA_CPPFLAGS += -DWITH_NETWORK
endif

OPENTYRIAN_VERSION := $(shell $(VCS_IDREV) 2>/dev/null && \
                              touch src/opentyrian_version.h)
ifneq ($(OPENTYRIAN_VERSION), )
    EXTRA_CPPFLAGS += -DOPENTYRIAN_VERSION='"$(OPENTYRIAN_VERSION)"'
endif

# -Wno-format-truncation only exists in GCC; Clang rejects it under -Werror
ifeq ($(findstring clang, $(shell $(CC) --version 2>/dev/null)), )
    WNO_FORMAT_TRUNCATION := -Wno-format-truncation
endif

CPPFLAGS ?= -MMD
CPPFLAGS += -DNDEBUG
CFLAGS ?= -pedantic \
          -Wall \
          -Wextra \
          $(WNO_FORMAT_TRUNCATION) \
          -Wno-missing-field-initializers \
          -O2
LDFLAGS ?=
LDLIBS ?=

ifeq ($(PLATFORM), EMSCRIPTEN)
    # Emscripten's SDL2 port.  -sASYNCIFY lets the game's blocking loops yield to the browser: under it,
    # SDL_Delay() becomes emscripten_sleep().
    SDL_CPPFLAGS := -sUSE_SDL=2
    SDL_LDFLAGS := -sUSE_SDL=2 \
                   -sASYNCIFY \
                   -sASYNCIFY_STACK_SIZE=262144 \
                   -sSTACK_SIZE=1048576 \
                   -sALLOW_MEMORY_GROWTH=1 \
                   -sENVIRONMENT=web \
                   -lidbfs.js \
                   --pre-js web/pre.js \
                   --js-library web/library.js \
                   --shell-file web/shell.html \
                   --preload-file $(DATA_DIR)@/data
    SDL_LDLIBS :=
else ifeq ($(WITH_NETWORK), true)
    SDL_CPPFLAGS := $(shell $(PKG_CONFIG) sdl2 SDL2_net --cflags)
    SDL_LDFLAGS := $(shell $(PKG_CONFIG) sdl2 SDL2_net --libs-only-L --libs-only-other)
    SDL_LDLIBS := $(shell $(PKG_CONFIG) sdl2 SDL2_net --libs-only-l)
else
    SDL_CPPFLAGS := $(shell $(PKG_CONFIG) sdl2 --cflags)
    SDL_LDFLAGS := $(shell $(PKG_CONFIG) sdl2 --libs-only-L --libs-only-other)
    SDL_LDLIBS := $(shell $(PKG_CONFIG) sdl2 --libs-only-l)
endif

ALL_CPPFLAGS = -DTARGET_$(PLATFORM) \
               -DTYRIAN_DIR='"$(TYRIAN_DIR)"' \
               $(EXTRA_CPPFLAGS) \
               $(SDL_CPPFLAGS) \
               $(CPPFLAGS)
ALL_CFLAGS = -std=iso9899:1999 \
             $(CFLAGS)
ALL_LDFLAGS = $(SDL_LDFLAGS) \
              $(LDFLAGS)
ALL_LDLIBS = -lm \
             $(SDL_LDLIBS) \
             $(LDLIBS)

###

.PHONY : all
all : $(TARGET)

.PHONY : debug
debug : CPPFLAGS += -UNDEBUG
debug : CFLAGS += -Werror
debug : CFLAGS += -O0
debug : CFLAGS += -g3
debug : all

.PHONY : installdirs
installdirs :
	mkdir -p $(DESTDIR)$(bindir)
	mkdir -p $(DESTDIR)$(docdir)
	mkdir -p $(DESTDIR)$(man6dir)
	mkdir -p $(DESTDIR)$(desktopdir)
	mkdir -p $(DESTDIR)$(icondir)/hicolor/22x22/apps
	mkdir -p $(DESTDIR)$(icondir)/hicolor/24x24/apps
	mkdir -p $(DESTDIR)$(icondir)/hicolor/32x32/apps
	mkdir -p $(DESTDIR)$(icondir)/hicolor/48x48/apps
	mkdir -p $(DESTDIR)$(icondir)/hicolor/128x128/apps

.PHONY : install
install : $(TARGET) installdirs
	$(INSTALL_PROGRAM) $(TARGET) $(DESTDIR)$(bindir)/
	$(INSTALL_DATA) README.md $(DESTDIR)$(docdir)/
	$(INSTALL_DATA) linux/man/opentyrian2000.6 $(DESTDIR)$(man6dir)/opentyrian2000$(man6ext)
	$(INSTALL_DATA) linux/opentyrian2000.desktop $(DESTDIR)$(desktopdir)/
	$(INSTALL_DATA) linux/icons/tyrian2000-22.png $(DESTDIR)$(icondir)/hicolor/22x22/apps/opentyrian2000.png
	$(INSTALL_DATA) linux/icons/tyrian2000-24.png $(DESTDIR)$(icondir)/hicolor/24x24/apps/opentyrian2000.png
	$(INSTALL_DATA) linux/icons/tyrian2000-32.png $(DESTDIR)$(icondir)/hicolor/32x32/apps/opentyrian2000.png
	$(INSTALL_DATA) linux/icons/tyrian2000-48.png $(DESTDIR)$(icondir)/hicolor/48x48/apps/opentyrian2000.png
	$(INSTALL_DATA) linux/icons/tyrian2000-128.png $(DESTDIR)$(icondir)/hicolor/128x128/apps/opentyrian2000.png

.PHONY : uninstall
uninstall :
	rm -f $(DESTDIR)$(bindir)/$(TARGET)
	rm -f $(DESTDIR)$(docdir)/README.md
	rm -f $(DESTDIR)$(man6dir)/opentyrian2000$(man6ext)
	rm -f $(DESTDIR)$(desktopdir)/opentyrian2000.desktop
	rm -f $(DESTDIR)$(icondir)/hicolor/22x22/apps/opentyrian2000.png
	rm -f $(DESTDIR)$(icondir)/hicolor/24x24/apps/opentyrian2000.png
	rm -f $(DESTDIR)$(icondir)/hicolor/32x32/apps/opentyrian2000.png
	rm -f $(DESTDIR)$(icondir)/hicolor/48x48/apps/opentyrian2000.png
	rm -f $(DESTDIR)$(icondir)/hicolor/128x128/apps/opentyrian2000.png

.PHONY : clean
clean :
	rm -f $(OBJS)
	rm -f $(DEPS)
	rm -f $(RES)
	rm -f $(TARGET)
	rm -f opentyrian2000.js opentyrian2000.wasm opentyrian2000.data

$(TARGET) : $(OBJS) $(RES) $(WEB_FILES)
	$(CC) $(ALL_CFLAGS) $(ALL_LDFLAGS) -o $@ $(OBJS) $(RES) $(ALL_LDLIBS)

-include $(DEPS)

obj/%.o : src/%.c
	@mkdir -p "$(dir $@)"
	$(CC) $(ALL_CPPFLAGS) $(ALL_CFLAGS) -c -o $@ $<

obj/resources.o : visualc/resources.rc visualc/tyrian.ico
	@mkdir -p "$(dir $@)"
	$(WINDRES) --include-dir visualc -i $< -o $@
