// Copyright © Tavian Barnes <tavianator@tavianator.com>
// SPDX-License-Identifier: 0BSD

#include "futex.h"
#include "atomic.h"
#include "spin.h"
#include <pthread.h>
#include <signal.h>
#include <stdalign.h>
#include <stdint.h>

struct waiter {
    pthread_t thread;
    atomic int *futex;
    struct waiter *prev, *next;
};

struct waitq {
    spinlock_t lock;
    struct waiter list;
};

#define TABLE_SIZE 16
struct waitq table[TABLE_SIZE];

static struct waitq *get_waitq(atomic int *futex) {
        // Use the address of the futex as the hash key
        uintptr_t i = (uintptr_t)futex;
        // These bits are always zero, shift them out
        i /= alignof(atomic int);
        return &table[i % TABLE_SIZE];
}

static struct waiter *waitq_head(struct waitq *waitq) {
	struct waiter *head = &waitq->list;
	if (!head->prev || !head->next) {
		head->prev = head->next = head;
	}
	return head;
}

void futex_wait(atomic int *futex, int value) {
	struct waitq *waitq = get_waitq(futex);
	spin_lock(&waitq->lock);

	struct waiter *head = waitq_head(waitq);

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

	// Block the signal in the current thread so we can wait for it
	sigset_t old_mask, mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGCONT);
	pthread_sigmask(SIG_BLOCK, &mask, &old_mask);

	while (load(futex, relaxed) == value) {
		// Unlock the wait queue before we sleep
		spin_unlock(&waitq->lock);
		// Sleep until we receive SIGCONT
		int sig;
		sigwait(&mask, &sig);
		// Re-lock the wait queue
		spin_lock(&waitq->lock);
	}

	// Restore the old signal mask
	pthread_sigmask(SIG_SETMASK, &old_mask, NULL);

	// Remove ourselves from the wait queue
	waiter.prev->next = waiter.next;
	waiter.next->prev = waiter.prev;

	spin_unlock(&waitq->lock);
}

void futex_wake(atomic int *futex) {
	struct waitq *waitq = get_waitq(futex);
	spin_lock(&waitq->lock);

	struct waiter *head = waitq_head(waitq);
	for (struct waiter *waiter = head->next; waiter != head; waiter = waiter->next) {
		if (waiter->futex == futex) {
			pthread_kill(waiter->thread, SIGCONT);
			break;
		}
	}

	spin_unlock(&waitq->lock);
}
