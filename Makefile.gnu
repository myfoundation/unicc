# Standard GNU Makefile for the generic development environment at
# Phorward Software (no autotools, etc. wanted in here).

CFLAGS 			= -g -I../phorward/src -DUTF8 -DUNICODE -DDEBUG -Wall $(CLOCAL)
LIBPHORWARD		= ../phorward/src/libphorward.a

SOURCES			= 	\
				mem.c \
				error.c \
				first.c \
				lalr.c \
				utils.c \
				string.c \
				integrity.c \
				virtual.c \
				rewrite.c \
				debug.c \
				lex.c \
				list.c \
				build.c \
				buildxml.c \
				main.c \
				xml.c

all: unicc

boot_clean:
	-rm min_lalr1/min_lalr1.o
	-rm parse_boot1.o parse_boot2.o parse_boot3.o
	-rm parse_boot1.c parse_boot2.c parse_boot2.h parse_boot3.c parse_boot3.h
	-rm unicc_boot1 unicc_boot2 unicc_boot3 boot_min_lalr1

clean: boot_clean
	-rm *.o
	-rm unicc

make_install:
	cp Makefile.gnu Makefile

make_uninstall:
	-rm -f Makefile

# --- UniCC Bootstrap phase 0 --------------------------------------------------
#
# First we need to compile min_lalr1, which is a stand-alone parser generator
# that was written for experimental reasons before UniCC, but is needed by
# UniCC to bootstrap.
#

boot_min_lalr1_SOURCES = min_lalr1/min_lalr1.c
boot_min_lalr1_OBJECTS = $(patsubst %.c,%.o,$(boot_min_lalr1_SOURCES))

boot_min_lalr1: $(boot_min_lalr1_OBJECTS)
	$(CC) -o $@ $(boot_min_lalr1_OBJECTS)

# --- UniCC Bootstrap phase 1 --------------------------------------------------
#
# This phase uses the experimental min_lalr1 Parser Generator to build a
# rudimentary parser for UniCC. min_lalr1 must be available and compiled
# with its delivered Makefile.gnu in a directory ../min_lalr from here.
#

unicc_boot1_SOURCES = parse_boot1.c $(SOURCES)
unicc_boot1_OBJECTS = $(patsubst %.c,%.o,$(unicc_boot1_SOURCES))

parse_boot1.c: parse.min boot_min_lalr1
	./boot_min_lalr1 parse.min >$@ 2>/dev/null

unicc_boot1: $(unicc_boot1_OBJECTS) $(LIBPHORWARD)
	$(CC) -o $@ $(unicc_boot1_OBJECTS) $(LIBPHORWARD)

# --- UniCC Bootstrap phase 2 --------------------------------------------------
#
# In this phase, the parser generated by min_lalr1 is will be used to parse the
# grammar definition of the UniCC parser (parse.par)
#

unicc_boot2_SOURCES = parse_boot2.c $(SOURCES)
unicc_boot2_OBJECTS = $(patsubst %.c,%.o,$(unicc_boot2_SOURCES))

parse_boot2.c parse_boot2.h: parse.par unicc_boot1
	./unicc_boot1 -svwb parse_boot2 parse.par

unicc_boot2: $(unicc_boot2_OBJECTS) $(LIBPHORWARD)
	$(CC) -o $@ $(unicc_boot2_OBJECTS) $(LIBPHORWARD)

# --- UniCC Bootstrap phase 3 --------------------------------------------------
#
# In this phase, the UniCC parser compiled by UniCC will be used to build
# itself.
#

unicc_boot3_SOURCES = parse_boot3.c $(SOURCES)
unicc_boot3_OBJECTS = $(patsubst %.c,%.o,$(unicc_boot3_SOURCES))

parse_boot3.c parse_boot3.h: parse.par unicc_boot2
	./unicc_boot2 -svwb parse_boot3 parse.par

unicc_boot3: $(unicc_boot3_OBJECTS) $(LIBPHORWARD)
	$(CC) -o $@ $(unicc_boot3_OBJECTS) $(LIBPHORWARD)

# --- UniCC Final Build --------------------------------------------------------
#
# Using the third bootstrap phase, the final UniCC executable is built.
#

unicc_SOURCES = parse.c $(SOURCES)
unicc_OBJECTS = $(patsubst %.c,%.o,$(unicc_SOURCES))

parse.c parse.h: parse.par unicc_boot3
	./unicc_boot3 -svwb parse parse.par

unicc: $(unicc_OBJECTS) $(LIBPHORWARD)
	$(CC) -o $@ $(unicc_OBJECTS) $(LIBPHORWARD)
	make -f Makefile.gnu boot_clean

# --- UniCC Documentation ------------------------------------------------------
#
# Now documentation generation follows, using txt2tags.
#

doc: unicc.1.man

unicc.1.man: unicc.t2t
	txt2tags -t man -o $@ $?

