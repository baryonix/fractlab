GFRACTLAB_PKG = gtk+-2.0 gthread-2.0
FRACTLAB_ZOOM_PKG = glib-2.0 gthread-2.0 libpng
FRACTLAB_IMAGE_PKG = glib-2.0 gthread-2.0 libpng
FRACTLAB_WORKER_PKG = glib-2.0 gthread-2.0 libpng
TEST_PARSER_PKG = glib-2.0 gthread-2.0
CC = gcc
FLEX = flex
BISON = bison
NASM = nasm
# Define MY_MPN_SUB_SLOW if it's faster on your machine (yes, despite the
# name, it might actually be!), or if your machine doesn't use two's complement,
# or if you have nails enabled in GMP.
# On Pentium4, _not_ defining MY_MPN_SUB_SLOW increases performance by ~5%.
COPTS = -O3 -march=core2 -Wall -g #-DMY_MPN_SUB_SLOW
#USE_IA32_ASM = i387
GMP_DIR = /opt/gmp
MPFR_DIR = $(GMP_DIR)
CFLAGS = -D_REENTRANT -I$(GMP_DIR)/include -I$(MPFR_DIR)/include -D_XOPEN_SOURCE=600 $(shell pkg-config --cflags $(GFRACTLAB_PKG) $(FRACTLAB_ZOOM_PKG)) $(COPTS) $(C_DIALECT)
GMP_LIBS = $(GMP_DIR)/lib/libgmp.a
MPFR_LIBS = $(MPFR_DIR)/lib/libmpfr.a
GFRACTLAB_LIBS = $(shell pkg-config --libs $(GFRACTLAB_PKG)) $(MPFR_LIBS) $(GMP_LIBS) -lpthread -lm
FRACTLAB_ZOOM_LIBS = $(shell pkg-config --libs $(FRACTLAB_ZOOM_PKG)) $(MPFR_LIBS) $(GMP_LIBS) -lpthread -lm
FRACTLAB_IMAGE_LIBS = $(shell pkg-config --libs $(FRACTLAB_IMAGE_PKG)) $(MPFR_LIBS) $(GMP_LIBS) -lpthread -lm
LISSAJOULIA_LIBS = $(shell pkg-config --libs $(FRACTLAB_ZOOM_PKG)) $(MPFR_LIBS) $(GMP_LIBS) -lpthread -lm
FRACTLAB_WORKER_LIBS = $(shell pkg-config --libs $(FRACTLAB_WORKER_PKG)) $(MPFR_LIBS) $(GMP_LIBS) -lpthread -lm
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

GFRACTLAB_OBJECTS = main.o coord_lex.yy.o coord_parse.tab.o file.o fractal-render.o gtkmandel.o util.o gui.o gui-mainwin.o gui-typedlg.o gui-infodlg.o gui-util.o misc-math.o fractal-math.o
FRACTLAB_ZOOM_OBJECTS = zoom.o coord_lex.yy.o coord_parse.tab.o file.o util.o fractal-render.o anim.o misc-math.o fractal-math.o render-png.o
FRACTLAB_IMAGE_OBJECTS = image.o coord_lex.yy.o coord_parse.tab.o file.o util.o fractal-render.o misc-math.o fractal-math.o render-png.o
LISSAJOULIA_OBJECTS = lissajoulia.o coord_lex.yy.o coord_parse.tab.o file.o util.o fractal-render.o anim.o misc-math.o fractal-math.o render-png.o
FRACTLAB_WORKER_OBJECTS = worker.o coord_lex.yy.o coord_parse.tab.o file.o util.o fractal-render.o misc-math.o fractal-math.o render-png.o
STUPIDMNG_OBJECTS = crc.o stupidmng.o
TEST_PARSER_OBJECTS = test_parser.o coord_lex.yy.o coord_parse.tab.o util.o file.o fractal-render.o fractal-math.o misc-math.o

ifeq ($(USE_IA32_ASM),i387)
CFLAGS += -DMANDELBROT_FP_ASM
GFRACTLAB_OBJECTS += ia32/mandel387.o
FRACTLAB_ZOOM_OBJECTS += ia32/mandel387.o
LISSAJOULIA_OBJECTS += ia32/mandel387.o
endif

all: gfractlab$(SUFFIX) fractlab-zoom$(SUFFIX) fractlab-image$(SUFFIX) lissajoulia$(SUFFIX) fractlab-worker$(SUFFIX) stupidmng$(SUFFIX)

gfractlab$(SUFFIX): $(GFRACTLAB_OBJECTS)
	$(CC) -o $@ $^ $(GFRACTLAB_LIBS)

fractlab-zoom$(SUFFIX): $(FRACTLAB_ZOOM_OBJECTS)
	$(CC) -o $@ $^ $(FRACTLAB_ZOOM_LIBS)

fractlab-image$(SUFFIX): $(FRACTLAB_IMAGE_OBJECTS)
	$(CC) -o $@ $^ $(FRACTLAB_IMAGE_LIBS)

lissajoulia$(SUFFIX): $(LISSAJOULIA_OBJECTS)
	$(CC) -o $@ $^ $(LISSAJOULIA_LIBS)

fractlab-worker$(SUFFIX): $(FRACTLAB_WORKER_OBJECTS)
	$(CC) -o $@ $^ $(FRACTLAB_WORKER_LIBS)

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
	-rm -f *.o ia32/*.o gfractlab$(SUFFIX) fractlab-zoom$(SUFFIX) fractlab-image$(SUFFIX) lissajoulia$(SUFFIX) fractlab-worker$(SUFFIX) stupidmng$(SUFFIX) test_parser$(SUFFIX)

distclean: clean
	-rm -f *.yy.[ch] *.tab.[ch]

newdeps:
	$(CC) -MM *.c >Makefile.deps

%_parse.tab.c %_parse.tab.h: %_parse.y
	$(BISON) -p $*_ -d -b $*_parse $<

%_lex.yy.c %_lex.yy.h: %_lex.l
	$(FLEX) -P$*_ -o$*_lex.yy.c --header-file=$*_lex.yy.h $<

include Makefile.deps
