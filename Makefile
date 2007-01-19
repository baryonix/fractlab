MANDEL_GTK_PKG = gtk+-2.0 gthread-2.0
MANDEL_ZOOM_PKG = glib-2.0 gthread-2.0 libpng
CC = gcc
LEX = lex
YACC = yacc
NASM = nasm
# Define MY_MPN_SUB_SLOW if it's faster on your machine (yes, despite the
# name, it might actually be!), or if your machine doesn't use two's complement,
# or if you have nails enabled in GMP.
# On Pentium4, _not_ defining MY_MPN_SUB_SLOW increases performance by ~5%.
COPTS = -O3 -march=pentium4 -Wall -g -std=c99 #-DMY_MPN_SUB_SLOW
USE_IA32_ASM = i387
GMP_DIR = /opt/gmp
MPFR_DIR = $(GMP_DIR)
CFLAGS = -D_REENTRANT -I$(GMP_DIR)/include -I$(MPFR_DIR)/include -D_XOPEN_SOURCE=600 $(shell pkg-config --cflags $(MANDEL_GTK_PKG) $(MANDEL_ZOOM_PKG)) $(COPTS)
GMP_LIBS = $(GMP_DIR)/lib/libgmp.a
MPFR_LIBS = $(MPFR_DIR)/lib/libmpfr.a
MANDEL_GTK_LIBS = $(shell pkg-config --libs $(MANDEL_GTK_PKG)) $(MPFR_LIBS) $(GMP_LIBS) -lpthread -lm
MANDEL_ZOOM_LIBS = $(shell pkg-config --libs $(MANDEL_ZOOM_PKG)) $(MPFR_LIBS) $(GMP_LIBS) -lpthread -lm
LISSAJOULIA_LIBS = $(shell pkg-config --libs $(MANDEL_ZOOM_PKG)) $(MPFR_LIBS) $(GMP_LIBS) -lpthread -lm

MANDEL_GTK_OBJECTS = main.o file.o cmdline.o mandelbrot.o gtkmandel.o gui.o util.o
MANDEL_ZOOM_OBJECTS = zoom.o file.o util.o mandelbrot.o anim.o
LISSAJOULIA_OBJECTS = lissajoulia.o file.o util.o mandelbrot.o anim.o
STUPIDMNG_OBJECTS = crc.o stupidmng.o
TEST_PARSER_OBJECTS = test_parser.o coord_lex.yy.o coord_parse.tab.o

ifeq ($(USE_IA32_ASM),i387)
CFLAGS += -DMANDELBROT_FP_ASM
MANDEL_GTK_OBJECTS += ia32/mandel387.o
MANDEL_ZOOM_OBJECTS += ia32/mandel387.o
LISSAJOULIA_OBJECTS += ia32/mandel387.o
endif

all: mandel-gtk mandel-zoom lissajoulia stupidmng

mandel-gtk: $(MANDEL_GTK_OBJECTS)
	$(CC) -o $@ $^ $(MANDEL_GTK_LIBS)

mandel-zoom: $(MANDEL_ZOOM_OBJECTS)
	$(CC) -o $@ $^ $(MANDEL_ZOOM_LIBS)

lissajoulia: $(LISSAJOULIA_OBJECTS)
	$(CC) -o $@ $^ $(LISSAJOULIA_LIBS)

stupidmng: $(STUPIDMNG_OBJECTS)
	$(CC) -o $@ $^

test_parser: $(TEST_PARSER_OBJECTS)
	$(CC) -o $@ $^ -ly -ll

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

.asm.o:
	$(NASM) -f elf -o $@ $<

%.tab.c %.tab.h: %.y
	$(YACC) -d -b $* $<

%.yy.c: %.l
	$(LEX) -o$@ $<

.PHONY: clean newdeps

.SUFFIXES: .asm

clean:
	-rm -f *.o ia32/*.o *.yy.c *.tab.[ch] mandel-gtk mandel-zoom lissajoulia stupidmng

newdeps:
	$(CC) -MM *.c >Makefile.deps

include Makefile.deps
