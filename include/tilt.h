/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 * SPDX-License-Identifier: MIT
 */

#ifndef TILT_H
#define TILT_H

/*******************************************************************************
 * @file tilt.h
 * @brief Lightweight pthread mutex interposition framework
 * @version 2.2
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
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
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
 * Tilt interface control
 * ****************************************************************************/
static bool _tilt_enabled = false;

/* Check wheter tilt is enabled. This function is used to fallback to pthread
 * interface at the end of the execution of a program. */
static inline bool
tilt_enabled(void)
{
    return __atomic_load_n(&_tilt_enabled, __ATOMIC_RELAXED);
}

/* Enable/disable tilt interface and fallback to pthread. The set of locks used
 * after disabling the interface should be disjoint from the set of locks while
 * tilt is enabled. This should only be used when the program is terminating.
 */
static inline void
tilt_control(bool enable)
{
    __atomic_store_n(&_tilt_enabled, enable, __ATOMIC_RELAXED);
}

/* *****************************************************************************
 * Opaque mutex and cond types
 *
 * Tilt internal representation of mutex and cond can take at most the
 * platform's size of the pthread counterparts.
 * ****************************************************************************/
struct _tilt_mutex {
    char impl[sizeof(pthread_mutex_t)];
};

#define TILT_MUTEX(m) ((struct tilt_mutex *)((struct _tilt_mutex *)mutex)->impl)

struct _tilt_cond {
    uint32_t val;
};

#define TILT_COND(c) ((struct _tilt_cond *)c)

/* cond_init initializes the pthread_cond replacement. _tilt_mutex_init and
 * other mutex functions are provided by the user */
static inline void
_tilt_cond_init(struct _tilt_cond *c)
{
    __atomic_store_n(&c->val, 0, __ATOMIC_SEQ_CST);
}

/* *****************************************************************************
 * Interposition helpers
 * ****************************************************************************/
#if defined(__linux__) || defined(__NetBSD__)
    #include <dlfcn.h>

    #define TILT_REAL(F, ...)                                                  \
        ((TILT_REAL_NAME(F) == NULL ?                                          \
              (TILT_REAL_NAME(F) = dlsym(RTLD_NEXT, #F)) :                     \
              0),                                                              \
         TILT_REAL_NAME(F)(__VA_ARGS__))
    #define TILT_REAL_NAME(F) __tilt_real_##F

    #define TILT_INTERPOSE(T, F, ...)                                          \
        static T (*TILT_REAL_NAME(F))(__VA_ARGS__);                            \
        T F(__VA_ARGS__)

#elif defined(__APPLE__)

    #define TILT_REAL(F, ...) F(__VA_ARGS__)
    #define TILT_FAKE_NAME(F) __tilt_fake_##F

    #define TILT_INTERPOSE(T, F, ...)                                          \
        T TILT_FAKE_NAME(F)(__VA_ARGS__);                                      \
        static struct {                                                        \
            const void *fake;                                                  \
            const void *real;                                                  \
        } _tilt_interpose_##F                                                  \
            __attribute__((used, section("__DATA,__interpose"))) = {           \
                (const void *)&TILT_FAKE_NAME(F), (const void *)&F};           \
        T TILT_FAKE_NAME(F)(__VA_ARGS__)
#else
    #error Unsupported platform
#endif

/* *****************************************************************************
 * ptread_mutex interposition
 * ****************************************************************************/
TILT_INTERPOSE(int, pthread_mutex_init, pthread_mutex_t *mutex,
               const pthread_mutexattr_t *attr)
{
    if (!tilt_enabled())
        return TILT_REAL(pthread_mutex_init, mutex, attr);
    memset(mutex, 0, sizeof(pthread_mutex_t));
    tilt_mutex_init(TILT_MUTEX(mutex));
    return 0;
}

TILT_INTERPOSE(int, pthread_mutex_destroy, pthread_mutex_t *mutex)
{
    if (!tilt_enabled())
        return TILT_REAL(pthread_mutex_destroy, mutex);
    tilt_mutex_destroy(TILT_MUTEX(mutex));
    return 0;
}

TILT_INTERPOSE(int, pthread_mutex_lock, pthread_mutex_t *mutex)
{
    if (!tilt_enabled())
        return TILT_REAL(pthread_mutex_lock, mutex);
    tilt_mutex_lock(TILT_MUTEX(mutex));
    return 0;
}

TILT_INTERPOSE(int, pthread_mutex_trylock, pthread_mutex_t *mutex)
{
    if (!tilt_enabled())
        return TILT_REAL(pthread_mutex_trylock, mutex);
    return tilt_mutex_trylock(TILT_MUTEX(mutex)) ? 0 : 1;
}

#if !defined(__APPLE__)
TILT_INTERPOSE(int, pthread_mutex_timedlock, pthread_mutex_t *mutex,
               const struct timespec *abstime)
{
    if (!tilt_enabled())
        return TILT_REAL(pthread_mutex_timedlock, mutex, abstime);

    assert(0 && "timedlock() not implemented");

    return 0;
}
#endif

TILT_INTERPOSE(int, pthread_mutex_unlock, pthread_mutex_t *mutex)
{
    if (!tilt_enabled())
        return TILT_REAL(pthread_mutex_unlock, mutex);
    tilt_mutex_unlock(TILT_MUTEX(mutex));
    return 0;
}

/* *****************************************************************************
 * ptread_cond interposition
 * ****************************************************************************/
TILT_INTERPOSE(int, pthread_cond_init, pthread_cond_t *cond,
               const pthread_condattr_t *attr)
{
    if (!tilt_enabled())
        return TILT_REAL(pthread_cond_init, cond, attr);
    _tilt_cond_init(TILT_COND(cond));
    return 0;
}

TILT_INTERPOSE(int, pthread_cond_destroy, pthread_cond_t *cond)
{
    if (!tilt_enabled())
        return TILT_REAL(pthread_cond_destroy, cond);
    return 0;
}

TILT_INTERPOSE(int, pthread_cond_wait, pthread_cond_t *cond,
               pthread_mutex_t *mutex)
{
    if (!tilt_enabled())
        return TILT_REAL(pthread_cond_wait, cond, mutex);
    struct _tilt_cond *c = TILT_COND(cond);
    uint32_t cur         = __atomic_load_n(&c->val, __ATOMIC_RELAXED);
    pthread_mutex_unlock(mutex);

    /* signal and broadcast "wake up" this loop */
    while (__atomic_load_n(&c->val, __ATOMIC_RELAXED) == cur) {
        sched_yield();
    }
    return pthread_mutex_lock(mutex);
}

TILT_INTERPOSE(int, pthread_cond_timedwait, pthread_cond_t *cond,
               pthread_mutex_t *mutex, const struct timespec *abstime)
{
    if (!tilt_enabled())
        return TILT_REAL(pthread_cond_timedwait, cond, mutex, abstime);
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

TILT_INTERPOSE(int, pthread_cond_signal, pthread_cond_t *cond)
{
    if (!tilt_enabled())
        return TILT_REAL(pthread_cond_signal, cond);
    struct _tilt_cond *c = TILT_COND(cond);
    __atomic_fetch_add(&c->val, 1, __ATOMIC_RELEASE);
    return 0;
}

TILT_INTERPOSE(int, pthread_cond_broadcast, pthread_cond_t *cond)
{
    if (!tilt_enabled())
        return TILT_REAL(pthread_cond_broadcast, cond);
    struct _tilt_cond *c = TILT_COND(cond);
    __atomic_fetch_add(&c->val, 1, __ATOMIC_RELEASE);
    return 0;
}

/* *****************************************************************************
 * Other interposed functions
 * ****************************************************************************/

/* Interposition of exit function disables tilt during exit. */
TILT_INTERPOSE(void, exit, int status)
{
    tilt_control(false);
    TILT_REAL(exit, status);
}

#if defined(__NetBSD__)
/* Interposition of atexit function to help initialization on NetBSD. */
TILT_INTERPOSE(int, atexit, void (*arg)(void))
{
    tilt_control(false);
    int r = TILT_REAL(atexit, arg);
    tilt_control(true);
    return r;
}
#endif

/* *****************************************************************************
 * Macro cleanup
 * ****************************************************************************/

#undef TILT_MUTEX
#undef TILT_COND
#undef TILT_INTERPOSE
#undef TILT_FAKE_NAME
#undef TILT_REAL_NAME
#undef TILT_REAL

#endif /* TILT_H */
