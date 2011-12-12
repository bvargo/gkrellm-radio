GKRELLMDIR=

PACKAGE=gkrellm-radio
VERSIONMAJOR=2
VERSIONMINOR=0
VERSIONREV=4

VERSION=$(VERSIONMAJOR).$(VERSIONMINOR).$(VERSIONREV)

DISTFILES=gkrellm_radio.c radio.c radio.h README Makefile CHANGES

CC=gcc
LDFLAGS=
OBJS=gkrellm_radio.o radio.o
PLUGIN_DIR ?= /usr/local/lib/gkrellm2/plugins
INSTALL = install -c
INSTALL_PROGRAM = $(INSTALL) -s
GTK_CONFIG = pkg-config gtk+-2.0
CFLAGS := ${CFLAGS} -fPIC -I$(GKRELLMDIR)/include `$(GTK_CONFIG) --cflags`  -DVERSION=\"$(VERSION)\" -Wall

ifdef WITH_LIRC
CFLAGS := ${CFLAGS} -DHAVE_LIRC
LDFLAGS:= ${LDFLAGS} -llirc_client
OBJS := ${OBJS} gkrellm_radio_lirc.o
DISTFILES := ${DISTFILES} gkrellm_radio_lirc.c
endif

LOCALEDIR ?= /usr/share/locale
ifeq ($(enable_nls),1)
  CFLAGS += -DENABLE_NLS -DLOCALEDIR=\"$(LOCALEDIR)\"
  export enable_nls
endif
PACKAGE ?= gkrellm-radio
CFLAGS += -DPACKAGE="\"$(PACKAGE)\"" 
export PACKAGE LOCALEDIR

radio.so: $(OBJS)
	$(CC) -shared -Wl -o radio.so $(OBJS) $(LDFLAGS) 
	(cd po && ${MAKE} all )

%.o: %.c
	$(CC) $(CFLAGS) -c $*.c

install: radio.so
	(cd po && ${MAKE} install)
	$(INSTALL_PROGRAM) -m 755 radio.so $(PLUGIN_DIR)

clean:
	rm -f radio.so $(OBJS) gkrellm_radio_lirc.o *~
	(cd po && ${MAKE} clean)

dist:
	rm -rf $(PACKAGE)-$(VERSION)
	mkdir $(PACKAGE)-$(VERSION)
	cp -a $(DISTFILES) $(PACKAGE)-$(VERSION)/
	tar cfz $(PACKAGE)-$(VERSION).tar.gz $(PACKAGE)-$(VERSION)/
	rm -rf $(PACKAGE)-$(VERSION)
