// Copyright © Tavian Barnes <tavianator@tavianator.com>
// SPDX-License-Identifier: 0BSD

#include "futex.h"
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

void futex_wait(atomic int *futex, int value) {
	syscall(SYS_futex, futex, FUTEX_WAIT_PRIVATE, value);
}

void futex_wake(atomic int *futex) {
	syscall(SYS_futex, futex, FUTEX_WAKE_PRIVATE, 1);
}
