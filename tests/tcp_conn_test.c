#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <assert.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <errno.h> //For errno - the error number
#include <netdb.h> //hostent
#include <arpa/inet.h>
#include <kstring.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <lthread.h>
#include <cdns.h>
#include <smux.h>

#define handle_error(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)

typedef struct _proxy_server {
    lthread_chan_t *accepts_ch;
    lthread_chan_t *die_ch;
    int is_die;
    int quiting;
    int listen_fd;
} proxy_server;
proxy_server g_srv = {0};

struct _proxy_conn {
    net_tcp_conn conn;
    lthread_chan_t* write_ch;
    lthread_chan_t* die_ch;
    int is_die;
    int is_priv;

    struct _proxy_conn* other;
};

typedef struct _proxy_conn proxy_conn_t;

//static functions
static int create_listener(char *ip, short port);
static void lt_accept_loop(void *arg);
static void lt_request_cli(void *arg);
static void lt_proxy_loop(void* arg);
static void lt_proxy_loop_read(void* arg);
static void lt_proxy_loop_write(void* arg);
void lt_main(void *arg);
static proxy_conn_t* proxy_conn_create(int fd, int is_priv);
static void proxy_conn_free(proxy_conn_t* conn);
static void server_init(proxy_server *srv);
//static void lt_signal(void *arg);
static void server_release(proxy_server *srv);

static int create_listener(char *ip, short port)
{
    int fd = 0, ret = 0;
    int opt;
    struct sockaddr_in sin;

    fd = lthread_socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (-1 == fd) {
        return -1;
    }

    opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt,sizeof(int)) == -1) {
        perror("failed to set SOREUSEADDR on socket");
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = PF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    /*if(!inet_pton(AF_INET, ip, &sin.sin_addr)) {
        perror("parse ip error");
        return -1;
    }*/

    sin.sin_port = htons(port);
    ret = bind(fd, (struct sockaddr *)&sin, sizeof(sin));
    if (-1 == ret) {
        close(fd);
        perror("cannot bind socket. closing.\n");
        return -1;
    }
    ret = listen(fd, 1024);
    if (-1 == ret) {
        close(fd);
        perror("cannot listen on socket. closing.\n");
        return -2;
    }

    return fd;
}

static void lt_accept_loop(void *arg)
{
    proxy_server *srv = (proxy_server*)arg;
    lthread_t *lt = NULL;
    proxy_conn_t *proxy = NULL;
    int ret;
    void* msgs[2];
    lthread_chan_t* chans[2];
    lthread_sel_t *sel = lthread_sel_create();
    chans[0] = srv->accepts_ch;
    chans[1] = srv->die_ch;

    while(!srv->is_die) {
        ret = chan_select(sel, chans, msgs, 0, 2, LTHREAD_FOREVER);
        switch(ret) {
        case 0:
            //recv ok
            fprintf(stderr, "recv accept channel ok\n");
            proxy = (proxy_conn_t*)msgs[0];
            if(NULL == proxy && chan_is_closed(msgs[0])) {
                ret = -10;
                break;
            }
            lthread_create(&lt, (void*)lt_request_cli, (void*)proxy);

            //yield myself
            lthread_sleep(0);

            break;
        case 1:
            //server die
            ret = -11;
            break;
        default:
            break;
        }

        if(ret < 0) {
            fprintf(stderr, "%s#%d ret=%d\n", __FUNCTION__, __LINE__, ret);
            break;
        }
    }

    lthread_sel_dispose(sel);
}

static void lt_request_cli(void *arg)
{
    int ret;
    lthread_t *lt = NULL;
    proxy_conn_t *priv_conn = proxy_conn_create(-1, 1);
    proxy_conn_t *proxy_conn = (proxy_conn_t*)arg;

    DEFINE_LTHREAD;
    lthread_detach();

    fprintf(stderr, "request new client\n");
    ret = tcp_dial_ipv4(&priv_conn->conn, "192.168.6.1:80");
    if(0 != ret) {
        proxy_conn_free(priv_conn);
        proxy_conn_free(proxy_conn);
        return;
    }

    fprintf(stderr, "dial priv conn ok\n");
    priv_conn->other = proxy_conn;
    proxy_conn->other = priv_conn;
    //lthread_create(&lt1, (void*)lt_proxy_loop, (void*)priv_conn);
    lthread_create(&lt, (void*)lt_proxy_loop, (void*)proxy_conn);
    lt_proxy_loop(priv_conn);

    lthread_join(lt, NULL, LTHREAD_FOREVER);

    void* data = NULL;
    kstring_t *buf = NULL;
    while(0 == chan_recv(priv_conn->write_ch, &data)) {
        if(data != NULL) {
            buf = (kstring_t*)data;
            ks_free(buf);
            free(buf);
        } else {
            //mark as end
            break;
        }
    }
    while(0 == chan_recv(proxy_conn->write_ch, &data)) {
        if(data != NULL) {
            buf = (kstring_t*)data;
            ks_free(buf);
            free(buf);
        } else {
            //mark as end
            break;
        }
    }
    proxy_conn_free(priv_conn);
    proxy_conn_free(proxy_conn);
}

static void lt_proxy_loop(void* arg)
{
    lthread_t *lt = NULL;
    proxy_conn_t *proxy = (proxy_conn_t*)arg;

    fprintf(stderr, "proxy loop\n");
    lthread_create(&lt, (void*)lt_proxy_loop_read, proxy);
    lt_proxy_loop_write(arg);

    lthread_join(lt, NULL, LTHREAD_FOREVER);
}

static void lt_proxy_loop_read(void* arg)
{
    proxy_conn_t *proxy = (proxy_conn_t*)arg;
    proxy_conn_t *other = proxy->other;
    proxy_server *srv = &g_srv;
    kstring_t *buf = NULL;
    int ret, n;
    lthread_chan_t* chans[3];
    void* msgs[3];
    lthread_sel_t* sel = lthread_sel_create();
    chans[0] = other->write_ch;
    chans[1] = proxy->die_ch;
    chans[2] = srv->die_ch;

    fprintf(stderr, "%s#%d started\n", __FUNCTION__, __LINE__);
    while(!proxy->is_die) {
        buf = (kstring_t*)calloc(1, sizeof(kstring_t));
        ks_resize(buf, 4096);
        n = net_conn_read(&proxy->conn.base, (uint8_t*)buf->s, (int)buf->m, (uint64_t)10000);
        if(n <= 0) {
            fprintf(stderr, "recv error n=%d priv=%d\n", n, proxy->is_priv);
            break;
        } else {
            buf->l = n;
            msgs[0] = buf;
            ret = chan_select(sel, chans, msgs, 1, 2, (uint64_t)10000);
            switch(ret) {
            case 0:
                //send ok
                buf = NULL;
                //fprintf(stderr, "send new writer\n");
                break;
            case 1:
                //proxy die
                ret = -10;
                break;
            case 2:
                //server die
                ret = -11;
                break;
            default:
                break;
            }
            if(ret < 0) {
                fprintf(stderr, "%s#%d error ret=%d is_priv=%d\n", __FUNCTION__, __LINE__, ret, proxy->is_priv);
                break;
            }
        }
    }

    if(NULL != buf) {
        ks_free(buf);
        free(buf);
        buf = NULL;
    }

    chan_close(other->write_ch);
    if(!proxy->is_die) {
        proxy->is_die = 1;
        chan_close(proxy->die_ch);
    }

    lthread_sel_dispose(sel);
}

static void lt_proxy_loop_write(void* arg)
{
    proxy_conn_t *proxy = (proxy_conn_t*)arg;
    //proxy_conn_t *other = proxy->other;
    proxy_server *srv = &g_srv;
    int ret;
    lthread_chan_t* chans[3];
    void* msgs[3];
    kstring_t *buf = NULL;
    lthread_sel_t* sel = lthread_sel_create();
    chans[0] = proxy->write_ch;
    chans[1] = proxy->die_ch;
    chans[2] = srv->die_ch;
    fprintf(stderr, "%s#%d started\n", __FUNCTION__, __LINE__);

    while(!proxy->is_die) {
        ret = chan_select(sel, chans, msgs, 0, 3, LTHREAD_FOREVER);
        switch(ret) {
        case 0:
            //recv channel ok
            buf = msgs[0];
            if(NULL == buf && chan_is_closed(chans[0])) {
                //channel closed
                ret = -10;
                fprintf(stderr, "%s#%d chan closed priv=%d\n", __FUNCTION__, __LINE__, proxy->is_priv);
                break;
            }
            assert(NULL != buf);

            //fprintf(stderr, "recv new writer\n");
            ret = net_conn_write(&proxy->conn.base, (uint8_t*)buf->s, buf->l, LTHREAD_FOREVER);
            if(ret <= 0) {
                fprintf(stderr, "write failed ret=%d priv=%d\n", ret, proxy->is_priv);
                ret = -11;
            }
            ks_free(buf);
            free(buf);
            buf = NULL;
            break;
        case 1:
            //proxy already die
            ret = -12;
            break;
        case 2:
            //server already die
            ret = -13;
            break;
        default:
            break;
        }

        //after switch
        if(ret < 0) {
            fprintf(stderr, "%s#%d error is_priv=%d ret=%d\n", __FUNCTION__, __LINE__, proxy->is_priv, ret);
            break;
        }
    }
    if(NULL != buf) {
        ks_free(buf);
        free(buf);
    }
    assert(0 == chan_size(chans[0]));
    if(!proxy->is_die) {
        proxy->is_die = 1;
        chan_close(proxy->die_ch);
    }
    net_conn_shutdown(&proxy->conn.base);
    lthread_sel_dispose(sel);
}

void lt_main(void *arg)
{
    int fd, opt = 1;
    proxy_server *srv = (proxy_server*)arg;
    lthread_t *lt_accept = NULL;
    struct sockaddr cin = {0};
    socklen_t addrlen = sizeof(struct sockaddr);
    proxy_conn_t *proxy = NULL;

    DEFINE_LTHREAD;
    lthread_detach();

    srv->listen_fd = create_listener("0.0.0.0", 9000);
    if(srv->listen_fd < 0) {
        exit(1);
    }
    fprintf(stderr, "listener creating :9000\n");

    lthread_create(&lt_accept, (void*)lt_accept_loop, (void*)srv);

    while(!srv->is_die) {
        fd = lthread_accept(srv->listen_fd, &cin, &addrlen);
        if(fd < 0) {
            perror("accept error");
            break;
        }
        if(srv->quiting) {
            lthread_close(fd);
            break;
        }
        if(srv->is_die) {
            //already die, close and break
            lthread_close(fd);
            fprintf(stderr, "server already die :9000\n");
            break;
        }

        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int)) == -1) {
            perror("failed to set SOREUSEADDR on socket");
            break;
        }

        fprintf(stderr, "accept new client\n");
        proxy = proxy_conn_create(fd, 0);
        if(0 != chan_send(srv->accepts_ch, proxy)) {
            //send failed, free proxy
            proxy_conn_free(proxy);
            break;
        }

        //yield myself
        lthread_sleep((uint64_t)0);
    }
    if(-1 != srv->listen_fd) {
        close(srv->listen_fd);
        srv->listen_fd = -1;
    }
    if(!srv->is_die) {
        srv->is_die = 1;
        chan_close(srv->die_ch);
        fprintf(stderr, "srv die\n");
    }

    fprintf(stderr, "lt_accept end\n");
    lthread_join(lt_accept, NULL, LTHREAD_FOREVER);

    //server release
    server_release(srv);
}

static proxy_conn_t* proxy_conn_create(int fd, int is_priv)
{
    proxy_conn_t* proxy = calloc(1, sizeof(proxy_conn_t));
    tcp_conn_init(&proxy->conn, fd);
    proxy->write_ch = chan_init(1024);
    proxy->die_ch = chan_init(0);
    proxy->is_priv = is_priv;
    return proxy;
}

static void proxy_conn_free(proxy_conn_t* proxy)
{
    net_conn_close(&proxy->conn.base);
    if(NULL != proxy->write_ch) {
        chan_dispose(proxy->write_ch);
    }
    if(NULL != proxy->die_ch) {
        chan_dispose(proxy->die_ch);
    }
    free(proxy);
}

static void server_init(proxy_server *srv)
{
    srv->accepts_ch = chan_init(0);
    srv->die_ch = chan_init(0);
    srv->is_die = 0;
}

static void server_release(proxy_server *srv)
{
    void* data = NULL;
    //only writer can close the channel
    chan_close(srv->accepts_ch);
    while(0 == chan_recv(srv->accepts_ch, &data)) {
        if(data != NULL) {
            proxy_conn_free(data);
        }
    }
}

#if 0
static void lt_signal(void *arg)
{
    sigset_t mask;
    int sfd;
    struct signalfd_siginfo fdsi;
    ssize_t s;
    proxy_server *srv = (proxy_server*)arg;

    DEFINE_LTHREAD;
    lthread_detach();

    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGQUIT);

    if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
        handle_error("sigprocmask");
    }

    sfd = signalfd(-1, &mask, SFD_NONBLOCK);
    if (sfd == -1) {
        handle_error("signalfd");
    }
    fprintf(stderr, "%s#%d started\n", __FUNCTION__, __LINE__);

    for (;;) {
        s = lthread_read(sfd, &fdsi, sizeof(struct signalfd_siginfo), LTHREAD_FOREVER);
        if (s != sizeof(struct signalfd_siginfo)) {
            handle_error("read");
        }
        fprintf(stderr, "%s#%d signal\n", __FUNCTION__, __LINE__);

        if (fdsi.ssi_signo == SIGINT) {
            fprintf(stderr, "Got SIGINT\n");
            chan_close(srv->die_ch);
            break;
        } else if (fdsi.ssi_signo == SIGQUIT) {
            fprintf(stderr, "Got SIGQUIT\n");
            chan_close(srv->die_ch);
            break;
        } else {
            printf("Read unexpected signal\n");
        }
    }
    close(sfd);
}
#endif

#if 0
static void sig_handler(int sig)
{
    proxy_server *srv = &g_srv;
    switch (sig) {
    case SIGINT:
    case SIGQUIT:
    case SIGUSR1:
        fprintf(stderr, "quiting...\n");
        srv->quiting = 1;
        break;
    default:
        break;
    }
}
#endif

int main(void)
{
    lthread_t *lt = NULL;
    proxy_server *srv = &g_srv;
    cdns_init();
    server_init(srv);

    signal(SIGPIPE, SIG_IGN);
    /*signal(SIGINT, sig_handler);
    signal(SIGQUIT, sig_handler);
    signal(SIGUSR1, sig_handler);*/

    lthread_create(&lt, (void*)lt_main, (void*)srv);

    //lt = NULL;
    //lthread_create(&lt, (void*)lt_signal, (void*)srv);

    lthread_run();
}

