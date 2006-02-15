PKG = gtk+-2.0 gthread-2.0
CC = gcc
CFLAGS = -I/opt/gmp/include -O3 -march=pentium4 -std=c99 -D_XOPEN_SOURCE -Wall -g $(shell pkg-config --cflags $(PKG))
LIBS = $(shell pkg-config --libs $(PKG)) /opt/gmp/lib/libgmp.a

MANDEL_GTK_OBJECTS = main.o file.o

mandel-gtk: $(MANDEL_GTK_OBJECTS)
	$(CC) -o $@ $^ $(LIBS)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean

clean:
	-rm -f *.o mandel-gtk

include Makefile.deps
