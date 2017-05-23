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
#include "kassert.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/syscall.h>
#include <linux/tcp.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include "lthread.h"

#define CPU_MAX (2)

typedef struct _lsn_cpu {
    int eventfd;
    pthread_mutex_t lock;
} lsn_cpu_t;

struct _lsn {
    size_t cpu_cnt;
    lsn_cpu_t *cpus;
} lsn;

static void eventfd_recv(void* arg) {
    size_t cpu = (size_t)arg;
    int lsn_fd, c;
    uint64_t u64 = 0;

    //DEFINE_LTHREAD;
    lthread_detach();

    lsn_fd = lsn.cpus[cpu].eventfd;
    fprintf(stderr, "cpu=%d started\n", (int)cpu);

    for(;;) {
        c = lthread_read(lsn_fd, &u64, sizeof(uint64_t), (uint64_t)0);
        if(c < 0) {
            fprintf(stderr, "cpu=%d read error\n", (int)cpu);
            break;
        }

        fprintf(stderr, "cpu=%d read c=%d\n", (int)cpu, c);
    }
}

static void eventfd_send(void* arg) {
    size_t cpu = (size_t)arg;
    int lsn_fd, c, i;
    uint64_t u64;

    //DEFINE_LTHREAD;
    lthread_detach();

    for(;;) {
        for(i = 1; i < lsn.cpu_cnt; i++) {
            lsn_fd = lsn.cpus[i].eventfd;
            u64 = 2;
            c = lthread_write(lsn_fd, &u64, sizeof(uint64_t));
            if(c < 0) {
                fprintf(stderr, "cpu=%d read error\n", (int)cpu);
                break;
            }

            fprintf(stderr, "cpu=%d write c=%d\n", (int)cpu, c);
            lthread_sleep((uint64_t)1000);
        }
    }
}

static void * run_percpu(void *arg) {
    lthread_t *lt = NULL;
    size_t cpu = (size_t)arg;

    lthread_create(&lt, (void*) eventfd_recv, arg);

    lthread_set_sched_cpu(cpu);
    lthread_run();

    return 0;
}

int main(int argc, char *argv[]) {
    int i, j;
    pthread_t pthread;
    lthread_t *lt = NULL;

    memset(&lsn, 0, sizeof(struct _lsn));
    lsn.cpu_cnt = (size_t)sysconf( _SC_NPROCESSORS_ONLN );
    if(lsn.cpu_cnt > CPU_MAX) {
        lsn.cpu_cnt = CPU_MAX;
    }
    lsn.cpus = calloc(lsn.cpu_cnt, sizeof(lsn_cpu_t));

    signal(SIGPIPE, SIG_IGN);
    srand(time(NULL));

    lsn.cpus[0].eventfd = eventfd(0, EFD_NONBLOCK);
    pthread_mutex_init(&lsn.cpus[0].lock, NULL);

    for(i = 1; i < lsn.cpu_cnt; i++) {
        lsn.cpus[i].eventfd = eventfd(0, EFD_NONBLOCK);
        pthread_mutex_init(&lsn.cpus[i].lock, NULL);
        if (pthread_create(&pthread, NULL, run_percpu, (void*)i) != 0) {
                return -1;
        }
        assert(pthread_detach(pthread) == 0);
    }

    lthread_create(&lt, (void*) eventfd_recv, 0);
    lthread_create(&lt, (void*) eventfd_send, 0);
    lthread_set_sched_cpu(0);
    lthread_run();
}

