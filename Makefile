# Copyright © Tavian Barnes <tavianator@tavianator.com>
# SPDX-License-Identifier: 0BSD

CFLAGS := -std=c17 -pthread -Wall -g -O3 -flto -D_GNU_SOURCE

ALL := futex_signal

OS := $(shell uname)

ifeq ($(OS),Linux)
ALL += futex_linux
endif

ifeq ($(OS),FreeBSD)
ALL += futex_freebsd
endif

all: $(ALL)
.PHONY: all

futex_%: main.c futex_%.c $(wildcard *.h)
	$(CC) $(CFLAGS) main.c futex_$*.c -o $@

clean:
	$(RM) $(ALL)
.PHONY: clean
