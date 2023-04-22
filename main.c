// Copyright © Tavian Barnes <tavianator@tavianator.com>
// SPDX-License-Identifier: 0BSD

#include "mutex.h"
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>

// sigwait() doesn't work on ignored signals on all systems
static void noop(int sig) { }

static mutex_t mutex = MUTEX_INITIALIZER;
static int count = 0;

static void *thread_fn(void *ptr) {
#define ITERS 1000000
	for (int i = 0; i < ITERS; ++i) {
		mutex_lock(&mutex);
		++count;
		mutex_unlock(&mutex);
	}

	return NULL;
}

int main(void) {
	signal(SIGCONT, noop);

#define THREADS 4
	pthread_t threads[THREADS];

	for (int i = 0; i < THREADS; ++i) {
		if (pthread_create(&threads[i], NULL, thread_fn, NULL) != 0) {
			return EXIT_FAILURE;
		}
	}

	for (int i = 0; i < THREADS; ++i) {
		if (pthread_join(threads[i], NULL) != 0) {
			return EXIT_FAILURE;
		}
	}

	if (count != THREADS * ITERS) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
