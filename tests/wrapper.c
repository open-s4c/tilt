/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 * SPDX-License-Identifier: MIT
 */
#include <tilt.h>
#include <stdbool.h>

typedef struct tilt_mutex tilt_mutex_t;

struct tilt_mutex {
    int lock;
};

static void
tilt_mutex_init(tilt_mutex_t *m)
{
    __atomic_store_n(&m->lock, 1, __ATOMIC_SEQ_CST);
}

static void
tilt_mutex_destroy(tilt_mutex_t *m)
{
    (void)m;
}

static bool
tilt_mutex_trylock(tilt_mutex_t *m)
{
    int expected = 0;
    return __atomic_compare_exchange_n(&m->lock, &expected, 1, false,
                                       __ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
}

static void
tilt_mutex_lock(tilt_mutex_t *m)
{
    int expected;
    do {
        expected = 0;
    } while (!__atomic_compare_exchange_n(&m->lock, &expected, 1, false,
                                          __ATOMIC_ACQUIRE, __ATOMIC_RELAXED));
}

static void
tilt_mutex_unlock(tilt_mutex_t *m)
{
    __atomic_store_n(&m->lock, 0, __ATOMIC_RELEASE);
}
