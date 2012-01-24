GKRELLMDIR=

PACKAGE=gkrellm-radio
VERSIONMAJOR=2
VERSIONMINOR=0
VERSIONREV=0

VERSION=$(VERSIONMAJOR).$(VERSIONMINOR).$(VERSIONREV)

DISTFILES=gkrellm_radio.c radio.c radio.h videodev.h README Makefile CHANGES

CC=gcc
LDFLAGS=
OBJS=gkrellm_radio.o radio.o
GTK_CONFIG = pkg-config gtk+-2.0
CFLAGS := ${CFLAGS} -fPIC -I$(GKRELLMDIR)/include `$(GTK_CONFIG) --cflags`  -DVERSION=\"$(VERSION)\" -Wall

ifdef WITH_LIRC
CFLAGS := ${CFLAGS} -DHAVE_LIRC
LDFLAGS:= ${LDFLAGS} -llirc_client
OBJS := ${OBJS} gkrellm_radio_lirc.o
DISTFILES := ${DISTFILES} gkrellm_radio_lirc.c
endif

radio.so: $(OBJS)
	$(CC) -shared -Wl -o radio.so $(OBJS) $(LDFLAGS) 

%.o: %.c
	$(CC) $(CFLAGS) -c $*.c

install: radio.so
	mkdir -p $$HOME/.gkrellm2/plugins
	cp radio.so $$HOME/.gkrellm2/plugins/

clean:
	rm -f radio.so $(OBJS) *~

dist:
	rm -rf $(PACKAGE)-$(VERSION)
	mkdir $(PACKAGE)-$(VERSION)
	cp -a $(DISTFILES) $(PACKAGE)-$(VERSION)/
	tar cfz $(PACKAGE)-$(VERSION).tar.gz $(PACKAGE)-$(VERSION)/
	rm -rf $(PACKAGE)-$(VERSION)
