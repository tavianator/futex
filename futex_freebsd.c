// Copyright © Tavian Barnes <tavianator@tavianator.com>
// SPDX-License-Identifier: 0BSD

#include "futex.h"
#include <stddef.h>
#include <sys/types.h>
#include <sys/umtx.h>

void futex_init(void) { }

void futex_wait(atomic int *futex, int value) {
	_umtx_op(futex, UMTX_OP_WAIT_UINT_PRIVATE, value, NULL, NULL);
}

void futex_wake(atomic int *futex) {
	_umtx_op(futex, UMTX_OP_WAKE_PRIVATE, 1, NULL, NULL);
}
