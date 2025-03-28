# Tilt

A lightweight framework for pthread mutex interposition. Tilt replaces `pthread_mutex` with user defined lock implementations, while incurring as little overhead as possible.
Yes, we are aware of [LiTL][].  In fact, Tilt is inspired by LiTL. However, in contrast to LiTL, Tilt introduces no dependencies whatsoever and runs on Arm or any architecture where pthread is available.  On the downside, Tilt lacks of the advanced features of LiTL.

[LiTL]: https://github.com/multicore-locks/litl

## Usage

Copy `include/tilt.h` into your project and create a file where your lock will be implemented, eg, `lock.c`.  If you prefer, you can use the provided `CMakeLists.txt` file instead of copying the header.  In `lock.c`, you have to define the following struct and functions:

```c
struct tilt_mutex;
static void tilt_mutex_init(tilt_mutex_t *m);
static void tilt_mutex_destroy(tilt_mutex_t *m);
static void tilt_mutex_lock(tilt_mutex_t *m);
static void tilt_mutex_unlock(tilt_mutex_t *m);
static bool tilt_mutex_trylock(tilt_mutex_t *m);
```

Here is a straightforward implementation of a "test-and-set" spinlock:

```c
#include <stdatomic.h>
#include <tilt.h>

typedef struct tilt_mutex {
    atomic_int lock;
} tilt_mutex_t;

static void tilt_mutex_init(tilt_mutex_t *m)
{ m->lock = 0; }

static void tilt_mutex_destroy(tilt_mutex_t *m)
{ m->lock = 0; }

static void tilt_mutex_lock(tilt_mutex_t *m)
{ while (atomic_exchange(&m->lock, 1)); }

static void
tilt_mutex_unlock(tilt_mutex_t *m)
{ m->lock = 0; }

static bool
tilt_mutex_trylock(tilt_mutex_t *m)
{ return 0 == atomic_exchange(&m->lock, 1); }
```

Compile your lock as a shared library:

```
gcc -shared -fPIC -o libmylock.so lock.c -Iinclude
```

Now, you can interpose pthread mutex with your lock using `LD_PRELOAD` as follows:

```
LD_PRELOAD=libmylock.so /path/to/some/multithreaded/program
```

See the [examples](examples) directory for more detailed examples.

## Limitations

The following assumptions must hold:

- The size of `struct tilt_mutex` should be less than or equal to the size of
  `pthread_mutex_t`.
- `pthread_mutex_t` initialized with the static initializer
  `PTHREAD_MUTEX_INITIALIZER` will be initialized with zeros.
- If a more complex behavior is necessary for the `struct tilt_mutex` initialization,
  `tilt_mutex_init` can be defined instead, but cannot be used statically, and
  the `attr` parameter will be ignored (it is not part of `tilt_mutex_init`)
  signature.  Notice that `tilt_mutex_init` must be defined in all cases, even
  if the application is not calling it explicitly.

Moreover, note these (fixable) limitations:
- the `pthread_cond` replacement is crude, using `sched_yield`.
- no support for user implementations of `pthread_cond`.
- `pthread_mutex_timedlock` is not implemented.
