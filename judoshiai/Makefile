#PROF=-pg
PROF=

include ../common/Makefile.inc

OBJDIR := $(JS_BUILD_DIR)/$(LASTPART)/$(OBJDIR)

all: $(OBJDIR) $(OBJDIR)/judoshiai$(SUFF)

$(OBJDIR):
	mkdir -p $(OBJDIR)

OBJS += $(OBJDIR)/main.o $(OBJDIR)/competitors.o $(OBJDIR)/sheets.o $(OBJDIR)/set-one-judoka.o \
	$(OBJDIR)/sqlite3.o $(OBJDIR)/popup-menu.o $(OBJDIR)/util.o $(OBJDIR)/db.o \
	$(OBJDIR)/categories.o $(OBJDIR)/log.o $(OBJDIR)/matches.o \
	$(OBJDIR)/names.o $(OBJDIR)/comm.o $(OBJDIR)/ip.o $(OBJDIR)/protocol.o \
	$(OBJDIR)/set-categories.o $(OBJDIR)/category-graph.o \
	$(OBJDIR)/completion.o $(OBJDIR)/another-shiai.o $(OBJDIR)/avl.o $(OBJDIR)/httpp.o \
	$(OBJDIR)/httpd.o $(OBJDIR)/import.o $(OBJDIR)/set-password.o \
	$(OBJDIR)/trees.o $(OBJDIR)/match-graph.o $(OBJDIR)/search.o $(OBJDIR)/menus.o \
	$(OBJDIR)/menuacts.o $(OBJDIR)/sql-dialog.o $(OBJDIR)/drawing.o $(OBJDIR)/print.o \
	$(OBJDIR)/print-texts.o $(OBJDIR)/db-competitors.o $(OBJDIR)/db-categories.o \
	$(OBJDIR)/db-matches.o $(OBJDIR)/db-table.o $(OBJDIR)/write-results.o \
	$(OBJDIR)/match-data.o $(OBJDIR)/serial.o \
	$(OBJDIR)/basic.tab.o $(OBJDIR)/lex.yy.o $(OBJDIR)/medal-matches.o $(OBJDIR)/db-validate.o \
	$(OBJDIR)/svg.o $(OBJDIR)/properties.o $(OBJDIR)/ftp.o $(OBJDIR)/language.o \
	$(OBJDIR)/custom-category-parser.o $(OBJDIR)/custom-category.o $(OBJDIR)/custom-category-svg-template.o

CFLAGS += -DAPPLICATION="\"judoshiai\""
CFLAGS += -DAPP_NUM=1

### Windows executable ###
ifeq ($(TGT),WIN32)
OBJS += judoshiai-icon.res

judoshiai-icon.res: judoshiai-icon.rc
	$(WINDRES) -O coff judoshiai-icon.rc judoshiai-icon.res

judoshiai-icon.rc: ../etc/judoshiai.ico
	echo "APPICONS ICON \"../etc/judoshiai.ico\"" >judoshiai-icon.rc

### Linux executable ###
else
PROF += -rdynamic
endif

$(OBJDIR)/judoshiai$(SUFF): $(OBJS)
	$(LD) $(PROF) -g -Wl,-rpath -Wl,bin $(OBJS) -o $(OBJDIR)/judoshiai$(SUFF) $(LIBS)

$(OBJDIR)/avl.o: ../common/avl.c
	${CC} $(PROF) -g $(CFLAGS) -c $< -o $@

$(OBJDIR)/ip.o: ../common/ip.c
	${CC} $(PROF) -g $(CFLAGS) -c $< -o $@

$(OBJDIR)/protocol.o: ../common/protocol.c
	${CC} $(PROF) -g $(CFLAGS) -c $< -o $@

$(OBJDIR)/binreloc.o: ../common/binreloc.c
	${CC} $(PROF) -g $(CFLAGS) -c $< -o $@

$(OBJDIR)/serial.o: ../common/serial.c
	${CC} $(PROF) -g $(CFLAGS) -c $< -o $@

$(OBJDIR)/language.o: ../common/language.c
	${CC} $(PROF) -g $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: %.c
	${CC} $(PROF) -g $(CFLAGS) -c $< -o $@

lex.yy.c: basic.l basic.tab.h
	flex basic.l

basic.tab.c basic.tab.h: basic.y
	bison -t -v -d basic.y

clean:
	rm -f $(OBJDIR)/* *.res *.rc *~ lex.yy.c basic.tab.c basic.tab.h

