#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <lthread.h>
#include "smux.h"
#include "cdns.h"

int net_conn_read(net_conn_base_t *obj, uint8_t* buf, int len, uint64_t timeout) {
    return (*obj->ops->read)(obj->priv, buf, len, timeout);
}

int net_conn_write(net_conn_base_t *obj, uint8_t* buf, int len, uint64_t timeout) {
    return (*obj->ops->write)(obj->priv, buf, len, timeout);
}

int net_conn_close(net_conn_base_t *obj) {
    return (*obj->ops->close)(obj->priv);
}

int net_conn_shutdown(net_conn_base_t *obj) {
    return (*obj->ops->shutdown)(obj->priv);
}

int net_conn_readfull(net_conn_base_t *obj, uint8_t* buf, int len, uint64_t timeout) {
    int m = 0, n = len, ret;
    while(n > 0) {
        ret = net_conn_read(obj, buf + m, n, timeout);
        if(ret <= 0) {
            return ret;
        } else {
            n -= ret;
            m += ret;
        }
    }

    return len;
}

static int tcp_conn_read(net_conn_base_t *obj, uint8_t* buf, int len, uint64_t timeout) {
    net_tcp_conn* tcp_conn = (net_tcp_conn*)obj;
    if(NULL == tcp_conn) {
        return -1;
    }

    return (int)lthread_read(tcp_conn->fd, buf, (size_t)len, (uint64_t)timeout);
}

static int tcp_conn_write(net_conn_base_t *obj, uint8_t* buf, int len, uint64_t timeout) {
    net_tcp_conn* tcp_conn = (net_tcp_conn*)obj;
    int ret, writed_n = 0;
    do {
        ret = (int)lthread_write(tcp_conn->fd, buf + writed_n, len);
        if(ret > 0) {
            writed_n += ret;
            len -= ret;
        }
    } while(ret > 0 && len > 0);

    return writed_n;
}

static int tcp_conn_close(net_conn_base_t *obj) {
    net_tcp_conn* tcp_conn = (net_tcp_conn*)obj;

    if(-1 != tcp_conn->fd) {
        lthread_close(tcp_conn->fd);
    }

    return 0;
}

static int tcp_conn_shutdown(net_conn_base_t* obj) {
    net_tcp_conn* tcp_conn = (net_tcp_conn*)obj;
    if(-1 != tcp_conn->fd) {
        shutdown(tcp_conn->fd, SHUT_RDWR);
    }

    return 0;
}

static net_conn_ops_t  tcp_conn_ops = {
    .read = tcp_conn_read,
    .write = tcp_conn_write,
    .close = tcp_conn_close,
    .shutdown = tcp_conn_shutdown,
};

int tcp_conn_init(net_tcp_conn *tcp_conn, int fd) {
    tcp_conn->base.ops = &tcp_conn_ops;
    tcp_conn->base.type_id = type_id_tcp_conn;
    tcp_conn->base.priv = tcp_conn;
    if(0 == fd) {
        tcp_conn->fd = -1;
    } else {
        tcp_conn->fd = fd;
    }
    return 0;
}

int tcp_dial_ipv4(net_tcp_conn *tcp_conn, const char* host_port) {
    char* port_str = strstr(host_port, ":");
    if(NULL == port_str) {
        return (-1);//port not found
    }
    short port = (short)atoi(port_str+1);
    int host_len = (port_str - host_port);
    char host[host_len+1];
    memcpy(host, host_port, host_len);
    host[host_len] = '\0';

    uint32_t ip = 0;
    struct sockaddr_in name = {0};
    if(!inet_pton(AF_INET, host, &name.sin_addr)) {
        ip = dns_lookup(host, host_len);
        if(0 == ip) {
            //dns not found
            return (-2);
        }
    } else {
        ip = name.sin_addr.s_addr;
    }

    int fd;
    memset(&name, 0, sizeof(struct sockaddr_in));
    name.sin_family = AF_INET;
    name.sin_port = htons(port);
    name.sin_addr.s_addr = ip;
    if ((fd = lthread_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        perror("Failed to create a new socket");
        return (-3);
    }

    int ret = lthread_connect(fd, (struct sockaddr*)&name,
            (socklen_t)sizeof(struct sockaddr_in), 5000);
    if(ret < 0) {
        close(fd); //close first!
        return -4;
    }

    tcp_conn->fd = fd;
    return 0;
}

