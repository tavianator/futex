// Copyright © Tavian Barnes <tavianator@tavianator.com>
// SPDX-License-Identifier: 0BSD

#include "futex.h"
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

void futex_init(void) { }

void futex_wait(atomic int *futex, int value) {
	syscall(SYS_futex, futex, FUTEX_WAIT_PRIVATE, value, NULL);
}

void futex_wake(atomic int *futex, int limit) {
	syscall(SYS_futex, futex, FUTEX_WAKE_PRIVATE, limit);
}
