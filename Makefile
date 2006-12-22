MANDEL_GTK_PKG = gtk+-2.0 gthread-2.0
MANDEL_ZOOM_PKG = glib-2.0 gthread-2.0 libpng
CC = gcc
LEX = lex
YACC = yacc
NASM = nasm
COPTS = -O3 -march=pentium4 -Wall -g
USE_IA32_ASM = i387
GMP_DIR = /opt/gmp
MPFR_DIR = $(GMP_DIR)
CFLAGS = -D_REENTRANT -I$(GMP_DIR)/include -I$(MPFR_DIR)/include -std=c99 -D_XOPEN_SOURCE $(shell pkg-config --cflags $(MANDEL_GTK_PKG) $(MANDEL_ZOOM_PKG)) $(COPTS)
GMP_LIBS = $(GMP_DIR)/lib/libgmp.a
MPFR_LIBS = $(MPFR_DIR)/lib/libmpfr.a
MANDEL_GTK_LIBS = $(shell pkg-config --libs $(MANDEL_GTK_PKG)) $(GMP_LIBS) -lpthread -lm
MANDEL_ZOOM_LIBS = $(shell pkg-config --libs $(MANDEL_ZOOM_PKG)) $(MPFR_LIBS) $(GMP_LIBS) -lpthread -lm

MANDEL_GTK_OBJECTS = main.o file.o cmdline.o mandelbrot.o gtkmandel.o gui.o util.o
MANDEL_ZOOM_OBJECTS = zoom.o file.o util.o mandelbrot.o
TEST_PARSER_OBJECTS = test_parser.o coord_lex.yy.o coord_parse.tab.o

ifeq ($(USE_IA32_ASM),i387)
CFLAGS += -DMANDELBROT_FP_ASM
MANDEL_GTK_OBJECTS += ia32/mandel387.o
MANDEL_ZOOM_OBJECTS += ia32/mandel387.o
endif

all: mandel-gtk mandel-zoom

mandel-gtk: $(MANDEL_GTK_OBJECTS)
	$(CC) -o $@ $^ $(MANDEL_GTK_LIBS)

mandel-zoom: $(MANDEL_ZOOM_OBJECTS)
	$(CC) -o $@ $^ $(MANDEL_ZOOM_LIBS)

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
	-rm -f *.o ia32/*.o *.yy.c *.tab.[ch] mandel-gtk mandel-zoom

newdeps:
	$(CC) -MM *.c >Makefile.deps

include Makefile.deps
