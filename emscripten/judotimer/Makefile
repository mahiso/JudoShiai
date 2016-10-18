all: ../../etc/timer.js ../../etc/timer.html

SRCS    = main.c stopwatch.c flags.c ../../common/avl.c comm.c ../common/font.c
CFLAGS  = -I. -I../common -I../../judotimer -I../../common
CFLAGS += -DJUDOTIMER=1 -O2 -g4 -s NO_EXIT_RUNTIME=1 -s ASSERTIONS=1
#CFLAGS += -s EXPORTED_FUNCTIONS="['_setscreensize']"

../../etc/timer.js: timer.js
	cp timer.js* ../../etc/

../../etc/timer.html: timer.html
	cp timer.html ../../etc/

timer.js: $(SRCS) Makefile
	emcc $(CFLAGS) $(SRCS) -o $@

timer.html: ../common/template.html
	sed 's/_APPLICATION_/timer/' $< > $@
