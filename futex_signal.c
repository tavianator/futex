// Copyright © Tavian Barnes <tavianator@tavianator.com>
// SPDX-License-Identifier: 0BSD

#include "futex.h"
#include "atomic.h"
#include "spin.h"
#include <pthread.h>
#include <signal.h>
#include <stdalign.h>
#include <stdint.h>

#define WAKE_SIGNAL SIGUSR1
static sigset_t wake_mask;

struct waiter {
	pthread_t thread;
	atomic int *futex;
	struct waiter *prev, *next;
};

struct waitq {
	alignas(64) spinlock_t lock;
	struct waiter list;
};

static void waitq_init(struct waitq *waitq) {
	spin_init(&waitq->lock);

	struct waiter *head = &waitq->list;
	head->prev = head->next = head;
}

#define TABLE_SIZE 64
struct waitq table[TABLE_SIZE];

void futex_init(void) {
	// Block the signal so we can wait for it
	sigemptyset(&wake_mask);
	sigaddset(&wake_mask, WAKE_SIGNAL);
	pthread_sigmask(SIG_BLOCK, &wake_mask, NULL);

	for (int i = 0; i < TABLE_SIZE; ++i) {
		waitq_init(&table[i]);
	}
}

static struct waitq *get_waitq(atomic int *futex) {
	// Use the address of the futex as the hash key
	uintptr_t i = (uintptr_t)futex;

	// https://nullprogram.com/blog/2018/07/31/
	i ^= i >> 16;
	i *= 0x45d9f3bU;
	i ^= i >> 16;
	i *= 0x45d9f3bU;
	i ^= i >> 16;

	return &table[i % TABLE_SIZE];
}

void futex_wait(atomic int *futex, int value) {
	struct waitq *waitq = get_waitq(futex);
	spin_lock(&waitq->lock);

	struct waiter *head = &waitq->list;

	// Store the wait queue entry on the stack
	struct waiter waiter = {
		.thread = pthread_self(),
		.futex = futex,
		.prev = head,
		.next = head->next,
	};

	// Insert the waiter into the list
	waiter.prev->next = &waiter;
	waiter.next->prev = &waiter;

	while (load(futex, relaxed) == value) {
		// Unlock the wait queue before we sleep
		spin_unlock(&waitq->lock);
		// Sleep until we receive WAKE_SIGNAL
		int sig;
		sigwait(&wake_mask, &sig);
		// Re-lock the wait queue
		spin_lock(&waitq->lock);
	}

	// Remove ourselves from the wait queue
	waiter.prev->next = waiter.next;
	waiter.next->prev = waiter.prev;

	spin_unlock(&waitq->lock);
}

void futex_wake(atomic int *futex, int limit) {
	struct waitq *waitq = get_waitq(futex);
	spin_lock(&waitq->lock);

	int count = 0;
	struct waiter *head = &waitq->list;
	for (struct waiter *waiter = head->next; waiter != head && count < limit; waiter = waiter->next) {
		if (waiter->futex == futex) {
			pthread_kill(waiter->thread, WAKE_SIGNAL);
			++count;
		}
	}

	spin_unlock(&waitq->lock);
}
