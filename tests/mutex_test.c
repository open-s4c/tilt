/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 * SPDX-License-Identifier: MIT
 */
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>

#define NTHREADS 3

pthread_mutex_t m;
int x;

void *
run_thread(void *arg)
{
    (void)arg;
    pthread_mutex_lock(&m);
    x++;
    pthread_mutex_unlock(&m);
    return 0;
}

int
main(const int argc, const char *argv[])
{
    pthread_t t[NTHREADS];
    for (int i = 0; i < NTHREADS; i++)
        pthread_create(&t[i], 0, run_thread, 0);
    for (int i = 0; i < NTHREADS; i++)
        pthread_join(t[i], 0);

    assert(x == NTHREADS);
    return 0;
}
