#include <vsync/spinlock/mcslock.h>
#include <vsync/spinlock/caslock.h>
#include <tilt.h>

typedef struct tilt_mutex {
    mcslock_t lock;
    caslock_t bit;
} tilt_mutex_t;

static void
tilt_mutex_init(tilt_mutex_t *m)
{
    mcslock_init(&m->lock);
    caslock_init(&m->bit);
}

static void
tilt_mutex_destroy(tilt_mutex_t *m)
{
}

static bool
tilt_mutex_trylock(tilt_mutex_t *m)
{
    return caslock_tryacquire(&m->bit);
}

static void
tilt_mutex_lock(tilt_mutex_t *m)
{
    mcs_node_t ctx;

    if (tilt_mutex_trylock(m))
        return;

    mcslock_acquire(&m->lock, &ctx);
    caslock_acquire(&m->bit);
    mcslock_release(&m->lock, &ctx);
}

static void
tilt_mutex_unlock(tilt_mutex_t *m)
{
    caslock_release(&m->bit);
}
