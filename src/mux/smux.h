#ifndef __SMUX_H_
#define __SMUX_H_

#include <stddef.h>
#include <time.h>
#include <kstring.h>
#include <queue.h>
#include <kvec.h>
#include <ringbuf.h>
#include <khash.h>
#include <lthread.h>

#if defined(USE_JEMALLOC)
#include <jemalloc/jemalloc.h>
#endif

/*
#include <mbedtls/config.h>
#include <mbedtls/platform.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/certs.h>
#include <mbedtls/x509.h>
#include <mbedtls/error.h>
#include <mbedtls/debug.h>
#include <mbedtls/timing.h>
*/

enum smux_type_id {
    type_id_invalid = 0,
    type_id_tcp_conn,
    type_id_tls_conn,
    type_id_smux_stream,
    type_id_smux_session,
    type_id_smux_write_req,
    type_id_max
};

struct _net_conn_ops;
struct _net_conn_base;
struct _net_tcp_conn;

typedef int (*fn_net_conn_read)(struct _net_conn_base* obj, uint8_t *buf, int len, uint64_t timeout);
typedef int (*fn_net_conn_write)(struct _net_conn_base* obj, uint8_t *buf, int len, uint64_t timeout);
typedef int (*fn_net_conn_close)(struct _net_conn_base* obj);
typedef int (*fn_net_conn_shutdown)(struct _net_conn_base* obj);
typedef struct _net_conn_ops {
    fn_net_conn_read    read;
    fn_net_conn_write   write;
    fn_net_conn_close   close;
    fn_net_conn_shutdown shutdown;
} net_conn_ops_t;

typedef struct _net_conn_base {
    int type_id;
    void *priv;//the implement object
    net_conn_ops_t *ops;
} net_conn_base_t;

typedef struct _net_tcp_conn {
    net_conn_base_t base;//must be in first field
    int fd;
} net_tcp_conn;

#define SMUX_LOG_LEVEL SMUX_DBG
#define smux_log(_level, _fmt, _args...) do {if(_level >= SMUX_LOG_LEVEL){ fprintf(stdout, _fmt, ##_args); }} while(0)

//tcp conn
int net_conn_read(net_conn_base_t *obj, uint8_t* buf, int len, uint64_t timeout);
int net_conn_write(net_conn_base_t *obj, uint8_t* buf, int len, uint64_t timeout);
int net_conn_close(net_conn_base_t *obj);
int net_conn_shutdown(net_conn_base_t *obj);
int net_conn_readfull(net_conn_base_t *obj, uint8_t* buf, int len, uint64_t timeout);
int tcp_conn_init(net_tcp_conn *tcp_conn, int fd);
int tcp_dial_ipv4(net_tcp_conn *tcp_conn, const char* host_port);

#endif

