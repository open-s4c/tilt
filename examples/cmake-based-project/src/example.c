#include <pthread.h>

int main() {
    pthread_mutex_t m;
    pthread_mutex_init(&m, NULL);
    /*
     * Notice the alternative code with pthread initializer can be used as well,
     * when the lock does not require a non-zero initialization:
     *   pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
     */

    pthread_mutex_trylock(&m);
    pthread_mutex_trylock(&m);
    pthread_mutex_unlock(&m);

    pthread_mutex_lock(&m);
    pthread_mutex_unlock(&m);
}
