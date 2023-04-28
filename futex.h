// Copyright © Tavian Barnes <tavianator@tavianator.com>
// SPDX-License-Identifier: 0BSD

#ifndef FUTEX_H
#define FUTEX_H

#include "atomic.h"

// Initialize futexes before use
void futex_init(void);

// Atomically check if `*futex == value`, and if so, go to sleep
void futex_wait(atomic int *futex, int value);

// Wake up `limit` threads currently waiting on `futex`
void futex_wake(atomic int *futex, int limit);

#define FUTEX_REQUEUE (USE_SIGNAL || USE_LINUX)

#if FUTEX_REQUEUE
// Wake up `limit` waiters, and re-queue the rest onto a different futex
void futex_requeue(atomic int *futex, int limit, atomic int *other);
#endif

#endif // FUTEX_H
