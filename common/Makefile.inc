SHIAI_VER_NUM=2.4-beta10
SHIAI_VERSION="\"$(SHIAI_VER_NUM)\""

ifndef JS_BUILD_DIR
  JS_BUILD_DIR=/home/$(USER)/js-build
endif

LASTPART := $(shell basename $(CURDIR))

GTKVER=3

OBJS=

### Target executable type ###
ifeq ($(TARGETOS),WIN32)
TGT=WIN32
else
ifeq ($(OS),Windows_NT)
TGT=WIN32
else
TGT=LINUX
JUDOPROXY=YES
endif
endif

####################################
# Default directories and programs #
####################################

### Cross compilation (host=Linux, target=Windows) ###
ifeq ($(TARGETOS),WIN32)

CC=i586-mingw32msvc-gcc
LD=i586-mingw32msvc-gcc
SUFF=.exe
DEVELDIR=/opt/win32/gtk
SOUNDDIR=/opt/win32/mpg123
RSVGDIR=/opt/win32/rsvg
CURLDIR=/opt/win32/curl
WEBKITDIR=/opt/win32/webkitgtk
SOAPDIR=/opt/win32/soup
RESHACKER=wine "C:\\bin\\ResHacker.exe"
WINDRES=wine "/opt/win32/MinGW/bin/windres.exe"
INNOSETUP=wine "/opt/win32/Inno Setup 5/ISCC.exe"
OBJDIR=obj-win32
RELEASEDIR=$(JS_BUILD_DIR)/release-win32

### Linux ###
else

CC=gcc
LD=gcc
SUFF=
DEVELDIR=
RESHACKER=
WINDRES=
INNOSETUP=
OBJDIR=obj-linux
RELEASEDIR=$(JS_BUILD_DIR)/release-linux

endif

###################
# CFLAGS and LIBS #
###################

### Cross compilation ###
ifeq ($(TARGETOS),WIN32)

CFLAGS=-Wall -g \
-I$(DEVELDIR)/include/gtk-$(GTKVER).0 \
-I$(DEVELDIR)/lib/gtk-$(GTKVER).0/include \
-I$(DEVELDIR)/include/atk-1.0 \
-I$(DEVELDIR)/include/cairo \
-I$(DEVELDIR)/include/pango-1.0 \
-I$(DEVELDIR)/include/glib-2.0 \
-I$(DEVELDIR)/lib/glib-2.0/include \
-I$(DEVELDIR)/include/freetype2 \
-I$(DEVELDIR)/include \
-I$(DEVELDIR)/include/libpng14   \
-I$(DEVELDIR)/include/cairo \
-I$(DEVELDIR)/include/freetype2 \
-I$(DEVELDIR)/include \
-I$(DEVELDIR)/include/libpng14   \
-I$(DEVELDIR)/include/gdk-pixbuf-2.0 \
-I/opt/win32/w32api/include \
-I/opt/win32/rsvg/include/librsvg-2.0 \
-I/opt/win32/curl/include \
-I../common -mms-bitfields \
-DSHIAI_VERSION=$(SHIAI_VERSION) \
-DENABLE_NLS \
-DGTKVER=$(GTKVER)

ifeq ($(JUDOPROXY),YES)
CFLAGS += -I/opt/win32/webkitgtk/include/webkitgtk-3.0 \
       -I/opt/win32/soup/include/libsoup-2.4
endif

LIBS=-L$(DEVELDIR)/lib $(DEVELDIR)/lib/glib-2.0.lib  \
-L/opt/win32/gtk/bin \
-lgtk-win32-$(GTKVER).0 -lgdk-win32-$(GTKVER).0 -latk-1.0 -lgio-2.0 -lgdk_pixbuf-2.0 \
-lpangowin32-1.0 -lgdi32 -lpangocairo-1.0 -lpango-1.0 -lgobject-2.0 \
-lgmodule-2.0 -lgthread-2.0 -lglib-2.0 -lintl -lcairo \
-L/opt/win32/rsvg/bin -lcroco-0.6-3 -lrsvg-2-2 -lxml2-2 \
-L/opt/win32/curl/bin -lcurl \
-L/opt/win32/w32api/lib -lws2_32 -mwindows

ifeq ($(JUDOPROXY),YES)
LIBS += -L/opt/win32/webkitgtk/lib -lwebkitgtk-3.0.dll \
     -L/opt/win32/soup/lib -lsoup-2.4.dll
endif

### Linux ###
else

PKGCONFIG=pkg-config

CFLAGS=-Wall $(shell $(PKGCONFIGPATH) $(PKGCONFIG) --cflags gtk+-$(GTKVER).0) \
       $(shell $(PKGCONFIGPATH) $(PKGCONFIG) --cflags cairo) \
       $(shell $(PKGCONFIGPATH) $(PKGCONFIG) --cflags librsvg-2.0) \
       $(shell $(PKGCONFIGPATH) $(PKGCONFIG) --cflags glib-2.0) \
       -DENABLE_BINRELOC -DENABLE_NLS -DGTKVER=$(GTKVER) \
       -I$(DEVELDIR)/include -I../common -DSHIAI_VERSION=$(SHIAI_VERSION)

LIBS=$(shell $(PKGCONFIGPATH) $(PKGCONFIG) --libs gtk+-$(GTKVER).0 gthread-2.0 cairo librsvg-2.0 glib-2.0) \
 $(shell curl-config --libs) -ldl

OBJS += $(OBJDIR)/binreloc.o

endif
