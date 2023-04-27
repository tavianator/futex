# Copyright © Tavian Barnes <tavianator@tavianator.com>
# SPDX-License-Identifier: 0BSD

CFLAGS := -std=c17 -pthread -Wall -g -O3 -flto -D_GNU_SOURCE

ALL := test_pthreads test_signal

OS := $(shell uname)

ifeq ($(OS),Linux)
ALL += test_linux
endif

ifeq ($(OS),FreeBSD)
ALL += test_freebsd
endif

all: $(ALL)
.PHONY: all

test_%: main.c futex_%.c $(wildcard *.h)
	$(CC) $(CFLAGS) main.c futex_$*.c -o $@

test_pthreads: CFLAGS += -DUSE_PTHREADS

clean:
	$(RM) $(ALL)
.PHONY: clean
