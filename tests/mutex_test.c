#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <limits.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <err.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/resource.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/syscall.h>
#include <linux/tcp.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <lthread.h>

lthread_mutex_t* m = NULL;
lthread_mutex_t* m2 = NULL;
static void lt_main1(void *arg) {
    DEFINE_LTHREAD;
    lthread_detach();

    lthread_mutex_lock(m);
    printf("%s#%d\n", __FUNCTION__, __LINE__);
    lthread_mutex_lock(m);
    printf("%s#%d\n", __FUNCTION__, __LINE__);
    lthread_mutex_lock(m);
    printf("%s#%d\n", __FUNCTION__, __LINE__);
    lthread_mutex_unlock(m);
    printf("%s#%d\n", __FUNCTION__, __LINE__);
    lthread_mutex_unlock(m);
    printf("%s#%d\n", __FUNCTION__, __LINE__);
    lthread_mutex_unlock(m);


    //do more unlock
    lthread_mutex_unlock(m);
    lthread_mutex_unlock(m);
    lthread_mutex_unlock(m);

    printf("%s#%d\n", __FUNCTION__, __LINE__);
    lthread_mutex_lock(m2);
}

static void lt_main2(void *arg) {
    DEFINE_LTHREAD;
    lthread_detach();

    lthread_mutex_lock(m);
    printf("%s#%d\n", __FUNCTION__, __LINE__);
    lthread_mutex_lock(m);
    printf("%s#%d\n", __FUNCTION__, __LINE__);
    lthread_mutex_lock(m);
    printf("%s#%d\n", __FUNCTION__, __LINE__);
    lthread_mutex_unlock(m);
    printf("%s#%d\n", __FUNCTION__, __LINE__);
    lthread_mutex_unlock(m);
    printf("%s#%d\n", __FUNCTION__, __LINE__);
    lthread_mutex_unlock(m);

    printf("%s#%d\n", __FUNCTION__, __LINE__);
    lthread_mutex_lock(m2);
}

int main(void) {
    lthread_t *lt1 = NULL, *lt2 = NULL;
    lthread_mutex_create(&m);
    lthread_mutex_create(&m2);
    lthread_create(&lt1, (void*)lt_main1, (void*)NULL);
    lthread_create(&lt2, (void*)lt_main2, (void*)NULL);
    lthread_run();
    return 0;
}

