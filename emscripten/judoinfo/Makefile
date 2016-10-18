all: ../../etc/info.js ../../etc/info.html

SRCS    = main.c comm.c ../../judoinfo/trees.c ../../common/avl.c ../common/font.c
CFLAGS  = -I. -I../common -I../../judoinfo -I../../common -O3
CFLAGS += -s ASSERTIONS=1

../../etc/info.js: info.js
	cp info.js* ../../etc/

../../etc/info.html: info.html
	cp info.html ../../etc/

info.js: $(SRCS) Makefile
	emcc $(CFLAGS) $(SRCS) -o $@

info.html: ../common/template.html
	sed 's/_APPLICATION_/info/' $< > $@
