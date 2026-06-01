ACLOCAL_AMFLAGS = -I m4

AM_CFLAGS = -O2 -Wall -Wextra -Werror -std=c11

bin_PROGRAMS = swizzle
bin_SCRIPTS = claude-lconf
swizzle_SOURCES = swizzle.c
dist_man_MANS = swizzle.1 claude-lconf.1

lib_LTLIBRARIES = libswizzle.la
libswizzle_la_SOURCES = libswizzle.c
libswizzle_la_LDFLAGS = -module -avoid-version
libswizzle_la_LIBADD = -ldl

check_PROGRAMS = probe
probe_SOURCES = tests/probe.c

TESTS = tests/test_swizzle.sh
TESTS_ENVIRONMENT = ROOT="$(abs_top_builddir)" PROBE="$(abs_top_builddir)/probe"
EXTRA_DIST = $(TESTS)
