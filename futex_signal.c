// Copyright © Tavian Barnes <tavianator@tavianator.com>
// SPDX-License-Identifier: 0BSD

#include "futex.h"
#include "atomic.h"
#include "spin.h"
#include <pthread.h>
#include <signal.h>
#include <stdalign.h>
#include <stdint.h>
#include <threads.h>

#if __linux__

static pid_t getpid_fast(void) {
	static atomic pid_t pid = 0;

	pid_t ret = load(&pid, relaxed);
	if (ret == 0) {
		ret = getpid();
		store(&pid, ret, relaxed);
	}

	return ret;
}

typedef pid_t tid_t;

static tid_t gettid_fast(void) {
	static thread_local tid_t tid = 0;
	if (tid == 0) {
		tid = gettid();
	}
	return tid;
}

// pthread_kill() is expensive on Linux, so use tgkill() directly
static void kill_thread(tid_t tid, int sig) {
	tgkill(getpid_fast(), tid, sig);
}

#else

typedef pthread_t tid_t;

static tid_t gettid_fast(void) {
	return pthread_self();
}

static void kill_thread(tid_t tid, int sig) {
	pthread_kill(tid, sig);
}

#endif

#define WAKE_SIGNAL SIGUSR1
static sigset_t wake_mask;

struct waiter {
	tid_t tid;
	uintptr_t addr;
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

static struct waitq *get_waitq(uintptr_t addr) {
	uintptr_t i = addr;

	// https://nullprogram.com/blog/2018/07/31/
	i ^= i >> 16;
	i *= 0x45d9f3bU;
	i ^= i >> 16;
	i *= 0x45d9f3bU;
	i ^= i >> 16;

	return &table[i % TABLE_SIZE];
}

void futex_wait(atomic int *futex, int value) {
	uintptr_t addr = (uintptr_t)futex;

	// Store the wait queue entry on the stack
	struct waiter waiter;
	waiter.tid = gettid_fast();
	waiter.addr = addr;

	struct waitq *waitq = get_waitq(addr);
	while (!spin_trylock(&waitq->lock)) {
		if (load(futex, relaxed) != value) {
			return;
		}
		spin_hint();
	}

	// Append the waiter to the list
	struct waiter *head = &waitq->list;
	waiter.prev = head->prev;
	waiter.next = head;
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
	uintptr_t addr = (uintptr_t)futex;
	struct waitq *waitq = get_waitq(addr);
	spin_lock(&waitq->lock);

	int count = 0;
	struct waiter *head = &waitq->list;
	for (struct waiter *waiter = head->next; waiter != head && count < limit; waiter = waiter->next) {
		if (waiter->addr != addr) {
			continue;
		}

		++count;
		if (count >= limit || waiter->next == head) {
			tid_t tid = waiter->tid;
			spin_unlock(&waitq->lock);
			kill_thread(tid, WAKE_SIGNAL);
			return;
		} else {
			kill_thread(waiter->tid, WAKE_SIGNAL);
		}
	}

	spin_unlock(&waitq->lock);
}
