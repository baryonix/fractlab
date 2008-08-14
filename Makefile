MANDEL_GTK_PKG = gtk+-2.0 gthread-2.0
MANDEL_ZOOM_PKG = glib-2.0 gthread-2.0 libpng
MANDEL_WORKER_PKG = glib-2.0 gthread-2.0 libpng
TEST_PARSER_PKG = glib-2.0 gthread-2.0
CC = gcc
FLEX = flex
BISON = bison
NASM = nasm
# Define MY_MPN_SUB_SLOW if it's faster on your machine (yes, despite the
# name, it might actually be!), or if your machine doesn't use two's complement,
# or if you have nails enabled in GMP.
# On Pentium4, _not_ defining MY_MPN_SUB_SLOW increases performance by ~5%.
COPTS = -O3 -march=pentium4 -fomit-frame-pointer -Wall -g #-DMY_MPN_SUB_SLOW
USE_IA32_ASM = i387
GMP_DIR = /opt/gmp
MPFR_DIR = $(GMP_DIR)
CFLAGS = -D_REENTRANT -I$(GMP_DIR)/include -I$(MPFR_DIR)/include -D_XOPEN_SOURCE=600 $(shell pkg-config --cflags $(MANDEL_GTK_PKG) $(MANDEL_ZOOM_PKG)) $(COPTS) $(C_DIALECT)
GMP_LIBS = $(GMP_DIR)/lib/libgmp.a
MPFR_LIBS = $(MPFR_DIR)/lib/libmpfr.a
MANDEL_GTK_LIBS = $(shell pkg-config --libs $(MANDEL_GTK_PKG)) $(MPFR_LIBS) $(GMP_LIBS) -lpthread -lm
MANDEL_ZOOM_LIBS = $(shell pkg-config --libs $(MANDEL_ZOOM_PKG)) $(MPFR_LIBS) $(GMP_LIBS) -lpthread -lm
LISSAJOULIA_LIBS = $(shell pkg-config --libs $(MANDEL_ZOOM_PKG)) $(MPFR_LIBS) $(GMP_LIBS) -lpthread -lm
MANDEL_WORKER_LIBS = $(shell pkg-config --libs $(MANDEL_WORKER_PKG)) $(MPFR_LIBS) $(GMP_LIBS) -lpthread -lm
TEST_PARSER_LIBS = $(shell pkg-config --libs $(TEST_PARSER_PKG)) $(MPFR_LIBS) $(GMP_LIBS) -lpthread -lm

ifneq ($(shell uname -s | grep CYGWIN_NT),)
OS = windows
else
OS = other
endif

ifeq ($(OS),windows)
OBJFORMAT = coff
SUFFIX = .exe
C_DIALECT = -std=gnu99
else
OBJFORMAT = elf
C_DIALECT = -std=c99
endif

MANDEL_GTK_OBJECTS = main.o coord_lex.yy.o coord_parse.tab.o file.o fractal-render.o gtkmandel.o util.o gui.o gui-mainwin.o gui-typedlg.o gui-infodlg.o gui-util.o misc-math.o fractal-math.o
MANDEL_ZOOM_OBJECTS = zoom.o coord_lex.yy.o coord_parse.tab.o file.o util.o fractal-render.o anim.o misc-math.o fractal-math.o render-png.o
LISSAJOULIA_OBJECTS = lissajoulia.o coord_lex.yy.o coord_parse.tab.o file.o util.o fractal-render.o anim.o misc-math.o fractal-math.o render-png.o
MANDEL_WORKER_OBJECTS = mandel-worker.o coord_lex.yy.o coord_parse.tab.o file.o util.o fractal-render.o misc-math.o fractal-math.o render-png.o
STUPIDMNG_OBJECTS = crc.o stupidmng.o
TEST_PARSER_OBJECTS = test_parser.o coord_lex.yy.o coord_parse.tab.o util.o file.o fractal-render.o fractal-math.o misc-math.o

ifeq ($(USE_IA32_ASM),i387)
CFLAGS += -DMANDELBROT_FP_ASM
MANDEL_GTK_OBJECTS += ia32/mandel387.o
MANDEL_ZOOM_OBJECTS += ia32/mandel387.o
LISSAJOULIA_OBJECTS += ia32/mandel387.o
endif

all: mandel-gtk$(SUFFIX) mandel-zoom$(SUFFIX) lissajoulia$(SUFFIX) mandel-worker$(SUFFIX) stupidmng$(SUFFIX)

mandel-gtk$(SUFFIX): $(MANDEL_GTK_OBJECTS)
	$(CC) -o $@ $^ $(MANDEL_GTK_LIBS)

mandel-zoom$(SUFFIX): $(MANDEL_ZOOM_OBJECTS)
	$(CC) -o $@ $^ $(MANDEL_ZOOM_LIBS)

lissajoulia$(SUFFIX): $(LISSAJOULIA_OBJECTS)
	$(CC) -o $@ $^ $(LISSAJOULIA_LIBS)

mandel-worker$(SUFFIX): $(MANDEL_WORKER_OBJECTS)
	$(CC) -o $@ $^ $(MANDEL_WORKER_LIBS)

stupidmng$(SUFFIX): $(STUPIDMNG_OBJECTS)
	$(CC) -o $@ $^

test_parser$(SUFFIX): $(TEST_PARSER_OBJECTS)
	$(CC) -o $@ $^ $(TEST_PARSER_LIBS)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

.asm.o:
	$(NASM) -f $(OBJFORMAT) -o $@ $<

.PHONY: clean distclean newdeps

.SUFFIXES: .asm

# This prevents make from removing intermediate files.
.SECONDARY:

clean:
	-rm -f *.o ia32/*.o mandel-gtk$(SUFFIX) mandel-zoom$(SUFFIX) lissajoulia$(SUFFIX) mandel-worker$(SUFFIX) stupidmng$(SUFFIX) test_parser$(SUFFIX)

distclean: clean
	-rm -f *.yy.[ch] *.tab.[ch]

newdeps:
	$(CC) -MM *.c >Makefile.deps

%_parse.tab.c %_parse.tab.h: %_parse.y
	$(BISON) -p $*_ -d -b $*_parse $<

%_lex.yy.c %_lex.yy.h: %_lex.l
	$(FLEX) -P$*_ -o$*_lex.yy.c --header-file=$*_lex.yy.h $<

include Makefile.deps
