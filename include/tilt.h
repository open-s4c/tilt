/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

#ifndef TILT_H
#define TILT_H

/*******************************************************************************
 * @file tilt.h
 * @brief Lightweight pthread mutex interposition framework
 * @version 2.1
 *
 * Tilt is a lightweight framework to intercept calls to pthread mutex and
 * replace them with user defined implementations.
 *
 * tilt.h is the only file you need to use Tilt. It contains the interposition
 * functions for `pthread_mutex_*` and `pthread_cond_*`. To use it, proceed as
 * follows:
 *
 * 1. include this file in a `.c`;
 * 2. define `struct tilt_mutex` to your likings;
 * 3. implement the lock interfaces:
 *   ```c
 *   void tilt_mutex_init(struct tilt_mutex *m);
 *   void tilt_mutex_destroy(struct tilt_mutex *m);
 *   void tilt_mutex_lock(struct tilt_mutex *m);
 *   void tilt_mutex_unlock(struct tilt_mutex *m);
 *   bool tilt_mutex_trylock(struct tilt_mutex *m);
 *   ```
 ******************************************************************************/

#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>

/* *****************************************************************************
 * Expected interface
 *
 * The following type and functions are expected to be implemented by the user.
 * ****************************************************************************/
struct tilt_mutex;

static void tilt_mutex_init(struct tilt_mutex *m);
static void tilt_mutex_destroy(struct tilt_mutex *m);
static void tilt_mutex_lock(struct tilt_mutex *m);
static void tilt_mutex_unlock(struct tilt_mutex *m);
static bool tilt_mutex_trylock(struct tilt_mutex *m);

/* *****************************************************************************
 * Opaque mutex and cond types
 * ****************************************************************************/
struct _tilt_mutex {
    char impl[sizeof(pthread_mutex_t)];
};

#define TILT_MUTEX(m) ((struct tilt_mutex *)((struct _tilt_mutex *)mutex)->impl)

struct _tilt_cond {
    uint32_t val;
};

#define TILT_COND(c) ((struct _tilt_cond *)c)

static inline void
_tilt_cond_init(struct _tilt_cond *c)
{
    __atomic_store_n(&c->val, 0, __ATOMIC_SEQ_CST);
}

/* *****************************************************************************
 * ptread_mutex interface
 * ****************************************************************************/
int
pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
    (void)attr;
    memset(mutex, 0, sizeof(pthread_mutex_t));
    tilt_mutex_init(TILT_MUTEX(mutex));
    return 0;
}

int
pthread_mutex_destroy(pthread_mutex_t *mutex)
{
    tilt_mutex_destroy(TILT_MUTEX(mutex));
    return 0;
}

int
pthread_mutex_lock(pthread_mutex_t *mutex)
{
    tilt_mutex_lock(TILT_MUTEX(mutex));
    return 0;
}

int
pthread_mutex_trylock(pthread_mutex_t *mutex)
{
    return tilt_mutex_trylock(TILT_MUTEX(mutex));
}

int
pthread_mutex_timedlock(pthread_mutex_t *mutex, const struct timespec *abstime)
{
    (void)mutex;
    (void)abstime;
    assert(0 && "timedlock() not implemented");
    return 0;
}

int
pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    tilt_mutex_unlock(TILT_MUTEX(mutex));
    return 0;
}

/* *****************************************************************************
 * ptread_cond interface
 * ****************************************************************************/
int
pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr)
{
    (void)attr;
    _tilt_cond_init(TILT_COND(cond));
    return 0;
}

int
pthread_cond_destroy(pthread_cond_t *cond)
{
    (void)cond;
    return 0;
}

int
pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
    struct _tilt_cond *c = TILT_COND(cond);
    uint32_t cur         = __atomic_load_n(&c->val, __ATOMIC_RELAXED);
    pthread_mutex_unlock(mutex);

    /* signal and broadcast "wake up" this loop */
    while (__atomic_load_n(&c->val, __ATOMIC_RELAXED) == cur) {
        sched_yield();
    }
    return pthread_mutex_lock(mutex);
}

int
pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
                       const struct timespec *abstime)
{
    struct timespec now;
    struct timeval ts;
    struct _tilt_cond *c = TILT_COND(cond);
    uint32_t cur         = __atomic_load_n(&c->val, __ATOMIC_RELAXED);
    pthread_mutex_unlock(mutex);

    while (__atomic_load_n(&c->val, __ATOMIC_RELAXED) == cur) {
        sched_yield();

        gettimeofday(&ts, NULL);
        now.tv_sec  = ts.tv_sec;
        now.tv_nsec = ts.tv_usec * 1000;

        if (now.tv_sec > abstime->tv_sec) {
            pthread_mutex_lock(mutex);
            return ETIMEDOUT;
        }
    }
    pthread_mutex_lock(mutex);
    return 0;
}

int
pthread_cond_signal(pthread_cond_t *cond)
{
    struct _tilt_cond *c = TILT_COND(cond);
    __atomic_fetch_add(&c->val, 1, __ATOMIC_RELEASE);
    return 0;
}

int
pthread_cond_broadcast(pthread_cond_t *cond)
{
    struct _tilt_cond *c = TILT_COND(cond);
    __atomic_fetch_add(&c->val, 1, __ATOMIC_RELEASE);
    return 0;
}

#endif /* TILT_H */
