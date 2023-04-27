// Copyright © Tavian Barnes <tavianator@tavianator.com>
// SPDX-License-Identifier: 0BSD

#ifndef MUTEX_H
#define MUTEX_H

#if USE_PTHREADS

#include <pthread.h>

#define mutex_t pthread_mutex_t
#define mutex_init(m) pthread_mutex_init(m, NULL)
#define MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#define mutex_trylock(m) (!pthread_mutex_trylock(m))
#define mutex_lock pthread_mutex_lock
#define mutex_unlock pthread_mutex_unlock

#else

#include "atomic.h"
#include "spin.h"
#include "futex.h"
#include <stdbool.h>

typedef struct {
	atomic int state;
} mutex_t;

enum {
	MUTEX_UNLOCKED,
	MUTEX_LOCKED,
	MUTEX_SLEEPING,
};

#define MUTEX_INITIALIZER { .state = MUTEX_UNLOCKED }

static inline void mutex_init(mutex_t *mutex) {
	atomic_init(&mutex->state, MUTEX_UNLOCKED);
}

static inline bool mutex_trylock(mutex_t *mutex) {
	int state = load(&mutex->state, relaxed);
	return state == MUTEX_UNLOCKED
		&& compare_exchange_weak(&mutex->state, &state, MUTEX_LOCKED, acquire, relaxed);
}

static inline void mutex_lock(mutex_t *mutex) {
	int state;

#define MUTEX_SPINS 128
	for (int i = 0; i < MUTEX_SPINS; ++i) {
		state = MUTEX_UNLOCKED;
		if (compare_exchange_weak(&mutex->state, &state, MUTEX_LOCKED, acquire, relaxed)) {
			return;
		}
		spin_hint();
	}

	if (state != MUTEX_SLEEPING) {
		state = exchange(&mutex->state, MUTEX_SLEEPING, acquire);
	}

	while (state != MUTEX_UNLOCKED) {
		futex_wait(&mutex->state, MUTEX_SLEEPING);
		state = exchange(&mutex->state, MUTEX_SLEEPING, acquire);
	}
}

static inline void mutex_unlock(mutex_t *mutex) {
	int state = exchange(&mutex->state, MUTEX_UNLOCKED, release);
	if (state == MUTEX_SLEEPING) {
		futex_wake(&mutex->state);
	}
}

#endif // USE_PTHREADS
#endif // MUTEX_H
