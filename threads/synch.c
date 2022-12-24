/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
   */

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/* One semaphore in a list. */
struct semaphore_elem {
	struct list_elem elem;              /* List element. */
	struct semaphore semaphore;         /* This semaphore. */
};

/* thread compare */
bool cmp_priority (struct list_elem *a, struct list_elem *b, void *aux UNUSED);

/* semaphore compare */
bool cmp_sem_priority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);

bool cmp_donate_priority (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);

/* 
 *  TODO : cmp_sem_priority 함수 
 *	a semaphore의 waiterlist중 max priority thread와 
 *	b semaphore의 waiterlist중 max priority thread 비교  
 */
bool cmp_sem_priority(const struct list_elem *a, const struct list_elem *b, void * aux UNUSED) {
	/* element a & b의 원본 소속 semaphore 찾아주기*/
	struct semaphore_elem *a_ss = list_entry(a, struct semaphore_elem, elem);
	struct semaphore_elem *b_ss = list_entry(b, struct semaphore_elem, elem);

	/* semaphore들 각각의 waiterlist 중 가장 우선순위 높은 elem */
	struct list_elem *a_elem = list_begin(&a_ss->semaphore.waiters);
	struct list_elem *b_elem = list_begin(&b_ss->semaphore.waiters);

	/* 찾은 elem 들의 원본 thread 찾기 */
	struct thread *t_a = list_entry(a_elem, struct thread, elem);
	struct thread *t_b = list_entry(b_elem, struct thread, elem);

	/* 원본 thread들의 priority 비교 */
	if (t_a->priority > t_b->priority) {
		return true;
	} else {
		return false; 
	}
}


/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
   decrement it.

   - up or "V": increment the value (and wake up one waiting
   thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value) {
	ASSERT (sema != NULL);

	sema->value = value;
	list_init (&sema->waiters);
}


/* 
 *	TODO : 수정 
 *	waiters list에 thread insert시, priority 순서대로
 *	이 함수는 sema를 '요청'및 '획득'후 value--
 */
/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. This is
   sema_down function. */
void
sema_down (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);
	ASSERT (!intr_context ());

	/* ----이 줄 밑으로 interrupt disable ------*/
	old_level = intr_disable ();
	while (sema->value == 0) {
		// list_push_back (&sema->waiters, &thread_current ()->elem);
		/* ---priority--- */
		// list_insert_ordered(&sema->waiters, &(thread_current()->elem), &cmp_sem_priority, NULL);
		list_insert_ordered(&sema->waiters, &(thread_current()->elem), cmp_priority, NULL);
		/* ----- */
		thread_block ();
	}
	sema->value--;
	/* --- 다시 interrupt 정상화 --- */
	intr_set_level (old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) {
	enum intr_level old_level;
	bool success;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (sema->value > 0)
	{
		sema->value--;
		success = true;
	}
	else
		success = false;
	intr_set_level (old_level);

	return success;
}


/* 
 *	TODO : 수정 
 *	waiters list를 priority 순서대로 정렬 
 * 	이 함수는 sema를 '반환'한고 value 1을 높인다.
 * 	semaphore 해제 후 priority preemption 기능 추가 
 */
/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void
sema_up (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (!list_empty (&sema->waiters)) {
		/* --- priority ---*/
		// list_sort(&sema->waiters, &cmp_sem_priority, NULL);
		list_sort(&sema->waiters, cmp_priority, NULL);
		/* --------------- */
		thread_unblock (list_entry (list_pop_front (&sema->waiters),
					struct thread, elem));
	}

	sema->value++;

	/* --- priority --- */
	test_max_priority();
	/* ---------------- */

	intr_set_level (old_level);
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) {
	struct semaphore sema[2];
	int i;

	printf ("Testing semaphores...");
	sema_init (&sema[0], 0);
	sema_init (&sema[1], 0);
	thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
	for (i = 0; i < 10; i++)
	{
		sema_up (&sema[0]);
		sema_down (&sema[1]);
	}
	printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) {
	struct semaphore *sema = sema_;
	int i;

	for (i = 0; i < 10; i++)
	{
		sema_down (&sema[0]);
		sema_up (&sema[1]);
	}
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock) {
	ASSERT (lock != NULL);

	lock->holder = NULL;
	sema_init (&lock->semaphore, 1);
}

/* 
 *	TODO : 수정 
 *	if lock is not available, lock addr 저장 
 *	현재 priority 저장 
 *	donated thread list 유지 
 *	이후 priority 기부한다. 
 *	lock을 점유하고 있는 thread와 요청하는 thread의 우선순위 비교
 * 	이후 priority donation 실행 
 */
/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
lock_acquire (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (!lock_held_by_current_thread (lock));
	
	/* -----------priority : donation----------*/
	/* current thread에 lock을 걸어준다 */
	/* current thread의 주인(master) */
	/* current의 priority와 master priority비교 후 donation 진행 */
	struct thread *curr = thread_current();

	/* MASTER의 존재여부 검사 */
	if (lock->holder != NULL) {
		struct thread *master = lock->holder;
		
		/* lock 획득 후 lock holder 갱신 */
		curr->lock_holder_ptr = lock;

		/* master waiter list에 curr 추가 */
		// printf("lock_acquire\n");
		list_insert_ordered(&master->donation_list, &curr->donate_list_id, cmp_donate_priority, NULL);  
		
		/* master에게 curr노예의 priority donation */
		donate_priority();
	}
	
	
	/* ---------------------------------------- */

	// go to waitingList
	sema_down (&lock->semaphore);

	/* ---donation--- */
	// 나는 이제 기다리는 사람이 없다?
	curr->lock_holder_ptr = NULL;
	/* -------------- */

	lock->holder = curr;
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock) {
	bool success;

	ASSERT (lock != NULL);
	ASSERT (!lock_held_by_current_thread (lock));

	success = sema_try_down (&lock->semaphore);
	if (success)
		lock->holder = thread_current ();
	return success;
}

/* 
 *	TODO : 수정 
 *	lock release 시, donation list 에서 lock을 들고 있는 thread 제거 
 *	이후 원래 priority set 
 *	donation list에서 thread delete 후 우선순위 다시 계산 
 *		--> call remove_with_lock(), refresh_priority()
 */
/* Releases LOCK, which must be owned by the current thread.
   This is lock_release function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (lock_held_by_current_thread (lock));
	
	/* current thread의 waiters list 에서 lock을 들고 있는 thread 제거 */
	remove_with_lock(lock);

	refresh_priority();

	lock->holder = NULL;
	sema_up (&lock->semaphore);
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) {
	ASSERT (lock != NULL);

	return lock->holder == thread_current ();
}



/* waiter list initialize */
/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond) {
	ASSERT (cond != NULL);

	list_init (&cond->waiters);
}


/* 
 *	TODO : 수정 
 *	반드시 LOCK 상태로 이 함수를 불러야한다 
 *	waiters list에 thread 삽입 시, priority order 로 	
 *	waiters list을 통해 signal이 오는지 가림(??)
 */
/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock) {
	struct semaphore_elem waiter;

	ASSERT (cond != NULL);
	ASSERT (lock != NULL);		// 반드시 lock 상태로 불린다
	ASSERT (!intr_context ());
	/* curr_thread가 lock을 가지고 있어서는 안된다 */
	ASSERT (lock_held_by_current_thread (lock));

	sema_init (&waiter.semaphore, 0);
	// list_push_back (&cond->waiters, &waiter.elem);
	/* --- priority --- */
	list_insert_ordered(&cond->waiters, &waiter.elem, cmp_sem_priority, NULL);
	/* ---------------- */
	lock_release (lock);
	sema_down (&waiter.semaphore);
	lock_acquire (lock);
}


/* 
 *	TODO : 수정 
 *	waiters list를 priority 순서대로 정렬 
 *	waiters list에서 기다리는 가장 높은 우선순위의 thread에게 signal
 */
/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	if (!list_empty (&cond->waiters)) {
		/* --- priority --- */
		list_sort(&cond->waiters, cmp_sem_priority, NULL);	
		/* ---------------- */
		sema_up (&list_entry (list_pop_front (&cond->waiters),
					struct semaphore_elem, elem)->semaphore);
	}
}


/* 
 *	condition variable에서 기다리는 모든 thread에게 signal 
 *	결국 waiter list에게 전부 신호를 보낸다는 뜻인듯(??)
 */
/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);

	while (!list_empty (&cond->waiters))
		cond_signal (cond, lock);
}
