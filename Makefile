PKG = gtk+-2.0 gthread-2.0
CC = gcc
NASM = nasm
USE_IA32_ASM = i387
CFLAGS = -I/opt/gmp/include -O3 -march=pentium4 -std=c99 -D_XOPEN_SOURCE -Wall -g $(shell pkg-config --cflags $(PKG))
LIBS = $(shell pkg-config --libs $(PKG)) /opt/gmp/lib/libgmp.a

MANDEL_GTK_OBJECTS = main.o file.o cmdline.o mandelbrot.o

ifeq ($(USE_IA32_ASM),i387)
CFLAGS += -DMANDELBROT_FP_ASM
MANDEL_GTK_OBJECTS += mandel387.o
endif

mandel-gtk: $(MANDEL_GTK_OBJECTS)
	$(CC) -o $@ $^ $(LIBS)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.asm
	$(NASM) -f elf $<

.PHONY: clean newdeps

clean:
	-rm -f *.o mandel-gtk

newdeps:
	$(CC) -MM *.c >Makefile.deps

include Makefile.deps
