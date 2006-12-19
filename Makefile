PKG = gtk+-2.0 gthread-2.0
CC = gcc
LEX = lex
YACC = yacc
NASM = nasm
USE_IA32_ASM = i387
COPTS = -O3 -march=pentium4 -Wall -g
CFLAGS = -I/opt/gmp/include -std=c99 -D_XOPEN_SOURCE $(shell pkg-config --cflags $(PKG)) $(COPTS)
LIBS = $(shell pkg-config --libs $(PKG)) /opt/gmp/lib/libgmp.a -lm

MANDEL_GTK_OBJECTS = main.o file.o cmdline.o mandelbrot.o gtkmandel.o gui.o util.o
TEST_PARSER_OBJECTS = test_parser.o coord_lex.yy.o coord_parse.tab.o

ifeq ($(USE_IA32_ASM),i387)
CFLAGS += -DMANDELBROT_FP_ASM
MANDEL_GTK_OBJECTS += ia32/mandel387.o
endif

mandel-gtk: $(MANDEL_GTK_OBJECTS)
	$(CC) -o $@ $^ $(LIBS)

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
	-rm -f *.o ia32/*.o *.yy.c *.tab.[ch] mandel-gtk

newdeps:
	$(CC) -MM *.c >Makefile.deps

include Makefile.deps
