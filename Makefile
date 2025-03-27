# Copyright © Tavian Barnes <tavianator@tavianator.com>
# SPDX-License-Identifier: 0BSD

# BSD make will chdir into ${.OBJDIR} by default, unless we tell it not to
.OBJDIR: .

RM ?= rm -f

CFLAGS := -std=c17 -pthread -Wall -g -O3 -flto -D_GNU_SOURCE

OS != uname

TARGETS,Linux := futex_linux
TARGETS,FreeBSD := futex_freebsd

TARGETS := futex_pthreads futex_signal ${TARGETS,${OS}}

all: ${TARGETS}
.PHONY: all

DEPS := main.c atomic.h cond.h futex.h mutex.h spin.h

futex_pthreads: futex_pthreads.c ${DEPS}
futex_signal: futex_signal.c ${DEPS}
futex_linux: futex_linux.c ${DEPS}
futex_freebsd: futex_freebsd.c ${DEPS}

${TARGETS}:
	${CC} ${CFLAGS} main.c ${@:futex_%=futex_%.c} -o $@

futex_pthreads: CFLAGS := ${CFLAGS} -DUSE_PTHREADS
futex_signal: CFLAGS := ${CFLAGS} -DUSE_SIGNAL
futex_linux: CFLAGS := ${CFLAGS} -DUSE_LINUX
futex_freebsd: CFLAGS := ${CFLAGS} -DUSE_FREEBSD

clean:
	${RM} ${TARGETS}
.PHONY: clean
