// Copyright © Tavian Barnes <tavianator@tavianator.com>
// SPDX-License-Identifier: 0BSD

#ifndef SPIN_H
#define SPIN_H

#include "atomic.h"
#include <stdbool.h>

typedef struct {
	atomic bool state;
} spinlock_t;

#define SPINLOCK_INITIALIZER { .state = false }

static inline void spin_init(spinlock_t *lock) {
	atomic_init(&lock->state, false);
}

static inline bool spin_trylock(spinlock_t *lock) {
	// Do a read first to avoid bouncing the cache line if it's locked
	return !load(&lock->state, relaxed) && !exchange(&lock->state, true, acquire);
}

#ifdef __has_builtin
#  if __has_builtin(__builtin_ia32_pause)
#    define spin_hint() __builtin_ia32_pause()
#  elif __has_builtin(__builtin_arm_yield)
#    define spin_hint() __builtin_arm_yield()
#  elif __has_builtin(__builtin_riscv_pause)
#    define spin_hint() __builtin_riscv_pause()
#  endif
#endif

#ifndef spin_hint
#  define spin_hint() ((void)0)
#endif

static inline void spin_lock(spinlock_t *lock) {
	// Due to the particular implementation of spin_trylock(), this becomes
	// a test-and-test-and-set (TTAS) loop. See https://rigtorp.se/spinlock/
	while (!spin_trylock(lock)) {
		spin_hint();
	}
}

static inline void spin_unlock(spinlock_t *lock) {
	store(&lock->state, false, release);
}

#endif // SPIN_H
