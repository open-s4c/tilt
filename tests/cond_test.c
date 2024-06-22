/*
 * Copyright (C) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 * SPDX-License-Identifier: MIT
 */
#include <pthread.h>
#include <stdlib.h>
#include <assert.h>

#define NTHREADS 3

pthread_mutex_t m;
pthread_cond_t c;
int x;

void *
run_thread(void *arg)
{
    (void)arg;
    pthread_mutex_lock(&m);
    x++;
    while (x != NTHREADS)
        pthread_cond_wait(&c, &m);
    pthread_mutex_unlock(&m);
    pthread_cond_signal(&c);
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
