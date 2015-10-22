include ../common/Makefile.inc

OBJDIR := $(JS_BUILD_DIR)/$(LASTPART)/$(OBJDIR)

all: $(OBJDIR) $(OBJDIR)/judotimer$(SUFF)

$(OBJDIR):
	mkdir -p $(OBJDIR)

OBJS += $(OBJDIR)/main.o $(OBJDIR)/menus.o $(OBJDIR)/stopwatch.o $(OBJDIR)/comm.o \
	$(OBJDIR)/ip.o $(OBJDIR)/protocol.o $(OBJDIR)/show-ad.o $(OBJDIR)/sound.o \
	$(OBJDIR)/video.o $(OBJDIR)/language.o

CFLAGS += -DAPPLICATION="\"judotimer\""
CFLAGS += -DAPP_NUM=2

### Windows executable ###
ifeq ($(TGT),WIN32)
CFLAGS += -I$(SOUNDDIR)/include
LIBS += -L$(SOUNDDIR)/bin -lmpg123-0 -lao-4
RPATH =
OBJS += judotimer-icon.res

judotimer-icon.res: judotimer-icon.rc
	$(WINDRES) -O coff judotimer-icon.rc judotimer-icon.res

judotimer-icon.rc:
	echo "APPICONS ICON \"../etc/judotimer.ico\"" >judotimer-icon.rc

### Linux executable ###
else
LIBS += -lmpg123 -lao
RPATH = -Wl,-rpath,/usr/lib/judoshiai/lib
endif

$(OBJDIR)/judotimer$(SUFF): $(OBJS)
	${LD} -g $(RPATH) $(OBJS) -o $(OBJDIR)/judotimer$(SUFF) $(LIBS)

$(OBJDIR)/ip.o: ../common/ip.c
	${CC} -g $(CFLAGS) -c $< -o $@

$(OBJDIR)/protocol.o: ../common/protocol.c
	${CC} -g $(CFLAGS) -c $< -o $@

$(OBJDIR)/binreloc.o: ../common/binreloc.c
	${CC} -g $(CFLAGS) -c $< -o $@

$(OBJDIR)/language.o: ../common/language.c
	${CC} $(PROF) -g $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: %.c
	${CC} -g $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJDIR)/* *.res *.rc *~
