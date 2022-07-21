# Standard GNU Makefile for the generic development environment at
# Phorward Software (no autotools, etc. wanted in here).

CFLAGS 			= -g -DUTF8 -DUNICODE -DDEBUG -Wall -I. $(CLOCAL)

SOURCES	= 	\
	lib/phorward.c \
	src/mem.c \
	src/error.c \
	src/first.c \
	src/lalr.c \
	src/utils.c \
	src/string.c \
	src/integrity.c \
	src/virtual.c \
	src/rewrite.c \
	src/debug.c \
	src/lex.c \
	src/list.c \
	src/build.c \
	src/buildxml.c \
	src/main.c \
	src/xml.c

all: unicc

boot_clean:
	-rm min_lalr1/min_lalr1.o
	-rm src/parse_boot1.o src/parse_boot2.o src/parse_boot3.o
	-rm src/parse_boot1.c src/parse_boot2.c src/parse_boot2.h src/parse_boot3.c src/parse_boot3.h
	-rm unicc_boot1 unicc_boot2 unicc_boot3 boot_min_lalr1

clean: boot_clean
	-rm src/*.o
	-rm unicc

src/proto.h: boot_clean
	lib/pproto *.c | awk "/int _parse/ { next } { print }" >$@

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

unicc_boot1_SOURCES = src/parse_boot1.c $(SOURCES)
unicc_boot1_OBJECTS = $(patsubst %.c,%.o,$(unicc_boot1_SOURCES))

src/parse_boot1.c: src/parse.min boot_min_lalr1
	./boot_min_lalr1 src/parse.min >$@ 2>/dev/null

unicc_boot1: $(unicc_boot1_OBJECTS)
	$(CC) -o $@ $(unicc_boot1_OBJECTS)

# --- UniCC Bootstrap phase 2 --------------------------------------------------
#
# In this phase, the parser generated by min_lalr1 is will be used to parse the
# grammar definition of the UniCC parser (parse.par)
#

unicc_boot2_SOURCES = src/parse_boot2.c $(SOURCES)
unicc_boot2_OBJECTS = $(patsubst %.c,%.o,$(unicc_boot2_SOURCES))

src/parse_boot2.c src/parse_boot2.h: src/parse.par unicc_boot1
	./unicc_boot1 -svwb src/parse_boot2 src/parse.par

unicc_boot2: $(unicc_boot2_OBJECTS)
	$(CC) -o $@ $(unicc_boot2_OBJECTS)

# --- UniCC Bootstrap phase 3 --------------------------------------------------
#
# In this phase, the UniCC parser compiled by UniCC will be used to build
# itself.
#

unicc_boot3_SOURCES = src/parse_boot3.c $(SOURCES)
unicc_boot3_OBJECTS = $(patsubst %.c,%.o,$(unicc_boot3_SOURCES))

src/parse_boot3.c parse_boot3.h: src/parse.par unicc_boot2
	./unicc_boot2 -svwb src/parse_boot3 src/parse.par

unicc_boot3: $(unicc_boot3_OBJECTS)
	$(CC) -o $@ $(unicc_boot3_OBJECTS)

# --- UniCC Final Build --------------------------------------------------------
#
# Using the third bootstrap phase, the final UniCC executable is built.
#

unicc_SOURCES = src/parse.c $(SOURCES)
unicc_OBJECTS = $(patsubst %.c,%.o,$(unicc_SOURCES))

#src/parse.c src/parse.h: src/parse.par unicc_boot3
#	./unicc_boot3 -svwb src/parse src/parse.par

unicc: $(unicc_OBJECTS)
	$(CC) -o $@ $(unicc_OBJECTS)

# --- UniCC Documentation ------------------------------------------------------
#
# Now documentation generation follows, using txt2tags.
#

doc: unicc.man

unicc.man: unicc.t2t
	txt2tags -t man -o $@ $?

# --- UniCC CI Test Suite ------------------------------------------------------

TESTPREFIX=test_
TESTEXPR="42 * 23 + 1337"
TESTRESULT="= 2303"

# C

$(TESTPREFIX)c_expr:
	./unicc -o $@ examples/expr.c.par
	cc -o $@  $@.c
	test "`echo $(TESTEXPR) | ./$@ -sl`" = $(TESTRESULT)

$(TESTPREFIX)c_ast:
	./unicc -o $@ examples/expr.ast.par
	cc -o $@ $@.c
	echo $(TESTEXPR) | ./$@ -sl

test_c: $(TESTPREFIX)c_expr $(TESTPREFIX)c_ast
	@echo "--- $@ succeeded ---"
	@rm $(TESTPREFIX)*


# C++

$(TESTPREFIX)cpp_expr:
	./unicc -o $@ examples/expr.cpp.par
	g++ -o $@  $@.cpp
	test "`echo $(TESTEXPR) | ./$@ -sl`" = $(TESTRESULT)

$(TESTPREFIX)cpp_ast:
	./unicc -l C++ -o $@ examples/expr.ast.par
	g++ -o $@ $@.cpp
	echo $(TESTEXPR) | ./$@ -sl

test_cpp: $(TESTPREFIX)cpp_expr $(TESTPREFIX)cpp_ast
	@echo "--- $@ succeeded ---"
	@rm $(TESTPREFIX)*

# Python

$(TESTPREFIX)py_expr:
	./unicc -o $@ examples/expr.py.par
	test "`python2 $@.py $(TESTEXPR) | head -n 1`" = $(TESTRESULT)
	test "`python3 $@.py $(TESTEXPR) | head -n 1`" = $(TESTRESULT)

$(TESTPREFIX)py_ast:
	./unicc -l Python -o $@ examples/expr.ast.par
	python2 $@.py $(TESTEXPR)
	python3 $@.py $(TESTEXPR)

test_py: $(TESTPREFIX)py_expr $(TESTPREFIX)py_ast
	@echo "--- $@ succeeded ---"
	@rm $(TESTPREFIX)*

# JavaScript

$(TESTPREFIX)js_expr:
	./unicc -wt examples/expr.js.par >$@.mjs
	@echo "var p = new Parser(); p.parse(process.argv[2]);" >>$@.mjs
	test "`node $@.mjs $(TESTEXPR) | head -n 1`" = $(TESTRESULT)

$(TESTPREFIX)js_ast:
	./unicc -wtl JavaScript examples/expr.ast.par >$@.mjs
	@echo "var p = new Parser(); var t = p.parse(process.argv[2]); t.dump();" >>$@.mjs
	node $@.mjs $(TESTEXPR)

test_js: $(TESTPREFIX)js_expr $(TESTPREFIX)js_ast
	@echo "--- $@ succeded ---"
	@rm $(TESTPREFIX)*

# Test

test: test_c test_cpp test_py test_js
	@echo "=== $+ succeeded ==="
