CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Werror -std=c11
LDFLAGS ?=

.PHONY: all clean test

all: swizzle libswizzle.so tests/probe

swizzle: swizzle.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

libswizzle.so: libswizzle.c
	$(CC) $(CFLAGS) -shared -fPIC $(LDFLAGS) -ldl -o $@ $<

tests/probe: tests/probe.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

test: all
	sh tests/test_swizzle.sh

clean:
	rm -f swizzle libswizzle.so tests/probe
