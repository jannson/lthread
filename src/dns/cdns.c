#define _XOPEN_SOURCE       /* See feature_test_macros(7) */
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
#include "kvec.h"
#include "lthread.h"
#include "cdns.h"


typedef struct
{
  uint16_t ID;
  uint16_t flags;
  uint16_t QDCOUNT;
  uint16_t ANCOUNT;
  uint16_t NSCOUNT;
  uint16_t ARCOUNT;
} __attribute__ ((packed)) DNSQuery;

typedef struct
{
  uint16_t QTYPE;
  uint16_t QCLASS;
} __attribute__ ((packed)) DNSQuestion;

typedef struct
{
  uint16_t TYPE;
  uint16_t CLASS;
  uint32_t TTL;
  uint16_t RDLENGTH;
} __attribute__ ((packed)) DNSAnswer;

#define DNS_FLAG_QR            (0x1 << 15)
#define DNS_FLAG_OPCODE_MASK   (0xF << 11)
#define DNS_FLAG_OPCODE_QUERY  (0x0 << 11)
#define DNS_FLAG_OPCODE_IQUERY (0x1 << 11)
#define DNS_FLAG_OPCODE_STATUS (0x2 << 11)
#define DNS_FLAG_AA            (0x1 << 10)
#define DNS_FLAG_TC            (0x1 <<  9)
#define DNS_FLAG_RD            (0x1 <<  8)
#define DNS_FLAG_RA            (0x1 <<  7)
#define DNS_FLAG_Z_MASK        (0x7 <<  4)
#define DNS_FLAG_RCODE_MASK    (0xF <<  0)
#define DNS_FLAG_RCODE_OK      (0x0)

#define DNS_TYPE_A   0x1
#define DNS_CLASS_IN 0x1

typedef kvec_t(uint32_t) ip_array;

typedef struct
{
    ip_array resolvers;
} ResolverManager;

static ResolverManager rev_mgr;

static int is_big_endian(void)
{
    union {
        uint32_t i;
        char c[4];
    } e = { 0x01000000 };

    return e.c[0];
}

//ipv4 only
void add_resolver(const char *resolver)
{
    uint32_t ip = 0;
    inet_pton(AF_INET, resolver, &ip);
    kv_push(uint32_t, rev_mgr.resolvers, ip);
}

static void resolv_conf_parse_line(char* start) {
    char *strtok_state = NULL, *token = NULL;
    static char* delims = " \t";
#define NEXT_TOKEN strtok_r(NULL, delims, &strtok_state)

    char* first_token = strtok_r(start, delims, &strtok_state);
    if(!first_token) {
        return;
    }

    if(!strcmp(first_token, "nameserver")) {
        token = NEXT_TOKEN;
        printf("nameserver=%s\n", token);
        add_resolver(token);
    } else if(!strcmp(first_token, "domain")) {
        token = NEXT_TOKEN;
    } else if(!strcmp(first_token, "search")) {
        while ((token = NEXT_TOKEN)) {
        }
    } else if(!strcmp(first_token, "options")) {
        while ((token = NEXT_TOKEN)) {
        }
    }

#undef NEXT_TOKEN
}

static int dns_resolv_parse(const char *filename) {
    struct stat st = {0};
    int fd = -1, n, r;
    uint8_t *resolv = NULL;
    char *start;
    int err = 0;

    fd = open(filename, O_RDONLY);
    if ( fd < 0 ) {
        //add_resolver("223.5.5.5");
        return 1;
    }

    if(0 != fstat(fd, &st)) {
        err = 2;
        goto out1;
    }

    if(!st.st_size) {
        err = 3;
        goto out1;
    }

    if(st.st_size > 65535) {
        err = 4;
        goto out1;
    }

    resolv = (uint8_t*) malloc((size_t)st.st_size + 1);
    if(!resolv) {
        err = 5;
        goto out1;
    }

    n = 0;
    while((r = read(fd, resolv+n, (size_t)st.st_size-n)) > 0) {
        n += r;
        if (n == st.st_size) {
            break;
        }
        assert(n < st.st_size);
    }

    if(r < 0) {
        err = 6;
        goto out2;
    }

    resolv[n] = 0;
    start = (char*)resolv;
    for(;;) {
        char *newline = strchr(start, '\n');
        if(!newline) {
            resolv_conf_parse_line(start);
            break;
        } else {
            *newline = 0;
            resolv_conf_parse_line(start);
            start = newline + 1;
        }
    }

out2:
    if(NULL != resolv) {
        free(resolv);
    }
out1:
    if(-1 != fd) {
        close(fd);
    }
    return err;
}

int cdns_init() {
    memset(&rev_mgr, 0, sizeof(rev_mgr));
    kv_init(rev_mgr.resolvers);
    return dns_resolv_parse("/etc/resolv.conf");
}

void cdns_deinit(void) {
    kv_destroy(rev_mgr.resolvers);
}

char * parse_dns_name(uint8_t *buffer, uint8_t *offset, int *len) {
    int dlen = 0, dmax = 256, followed = 0;
    //char* domain = (char*)malloc(dmax+1);

    *len = 1;

    while(*offset > 0) {
        if(!followed && (*offset & 0xC0) == 0xC0) {
            followed = 1;
            offset = buffer + sizeof(DNSQuery) + (*offset & ~0xC0);
            ++(*len);
            continue;
        }

        if(*offset > dmax) {
            assert(*offset < 4096);
            //domain = realloc(domain, *offset * 2 + 1);
        }

        //memcpy(domain + dlen, (char*)&offset[1], *offset);
        dlen += (int)*offset;

        if(!followed) {
            *len += (*offset + 1);
        }

        offset += (*offset + 1);

        if(*offset > 0) {
            //domain[dlen] = '.';
            dlen++;
        }
    }

    //domain[dlen] = '\0';
    return NULL;
}

#if 0
uint32_t dns_lookup(const char* host, int host_len) {
    uint32_t ipv4 = 0;
    lthread_compute_begin();
    ipv4 = hostname_to_ip4_async(host, host_len);
    lthread_compute_end();

    return ipv4;
}
#endif

uint32_t dns_lookup(const char * host, int host_len) {
    char *domain;
    uint32_t dns;
    int i, size, len, rbuf_size = 2048;
    int s = -1;
    uint32_t ipv4 = 0;
    struct sockaddr_in dest;
    DNSQuestion *question;

    int buf_len = sizeof(DNSQuery) + host_len + 2 + sizeof(DNSQuestion);
    uint8_t *rbuf, *sbuf = malloc(buf_len);
    DNSQuery *query = (DNSQuery*)sbuf;

    rbuf = malloc(rbuf_size);
    if(NULL == sbuf || NULL == rbuf)  {
        fprintf(stderr, "dns lookup malloc error\n");
        goto OUT;
    }

    memset(sbuf, 0, buf_len);

    query->ID      = rand() % UINT16_MAX;
    query->flags   = DNS_FLAG_OPCODE_QUERY | DNS_FLAG_RD;
    query->QDCOUNT = 1;

    /* add the hostname */
    memcpy(sbuf + sizeof(DNSQuery) + 1, host, host_len);
    sbuf[sizeof(DNSQuery) + 1 + host_len] = '\0';

    uint8_t *ptr = sbuf + sizeof(DNSQuery) + 1;
    len = 0;
    while(*ptr != '\0') {
        if (ptr[len] == '.' || ptr[len] == '\0')
        {
            ptr[-1] = len;
            ptr    += len + 1;
            len     = 0;
            continue;
        }
        ++len;
    }

    /* set the question type */
    question = (DNSQuestion*)(sbuf + sizeof(DNSQuery) + host_len + 2);
    question->QTYPE  = DNS_TYPE_A;
    question->QCLASS = DNS_CLASS_IN;
    if (!is_big_endian()) {
        swab(query, query, sizeof(DNSQuery));
        swab(question, question, sizeof(DNSQuestion));
    }

    for(i = 0; i < kv_size(rev_mgr.resolvers); i++) {
        dns = kv_A(rev_mgr.resolvers, i);
        if(-1 == (s = lthread_socket(AF_INET, SOCK_DGRAM, IPPROTO_IP))) {
            perror("udp socket error");
            goto OUT;
        }
        //fprintf(stderr, "s=-1x%08x\n", s);

        memset((char*)&dest, 0, sizeof(dest));
        dest.sin_family = AF_INET;
        dest.sin_port = htons(53);
        dest.sin_addr.s_addr = dns;

        if(lthread_connect(s, (struct sockaddr *)&dest, sizeof(dest), 1000) < 0) {
            lthread_close(s);
            perror("udp connect error");
            continue;
        }

        if(lthread_send(s, sbuf, buf_len, 0) < buf_len) {
            lthread_close(s);
            perror("sendto error");
            continue;
        }

        size = lthread_recv(s, rbuf, rbuf_size, 0, 3000);
        //printf("recvfrom size=%d\n", size);
        if(size > 0) {
            query = (DNSQuery*) rbuf;
            if(!is_big_endian()) {
                swab(query, query, sizeof(DNSQuery));
            }

            if (!(query->flags & DNS_FLAG_QR) || (query->flags & DNS_FLAG_RCODE_MASK) != DNS_FLAG_RCODE_OK) {
                lthread_close(s);
                fprintf(stderr, "dns flag error 0x%08x\n", query->flags);
                continue;
            }

            uint8_t *offset = rbuf + sizeof(DNSQuery);
            for(i = 0; i < query->QDCOUNT; ++i) {
                domain = parse_dns_name((uint8_t*)rbuf, offset, &len);
                offset += len + sizeof(DNSQuestion);

                //printf("domain=%s\n",  domain);
                if(NULL != domain) {
                    free(domain);
                }
            }

            for(i = 0; i < query->ANCOUNT; ++i) {
                domain = parse_dns_name((uint8_t*)rbuf, offset, &len);
                offset += len;

                //printf("domain=%s\n",  domain);
                if(NULL != domain) {
                    free(domain);
                }

                DNSAnswer *answer = (DNSAnswer*)offset;
                if(!is_big_endian()) {
                    swab(&answer->TYPE, &answer->TYPE, sizeof(uint16_t));
                    swab(&answer->CLASS, &answer->CLASS, sizeof(uint16_t));
                    swab(&answer->RDLENGTH, &answer->RDLENGTH, sizeof(uint16_t));
                    answer->TTL =
                        ((answer->TTL & 0xFF000000) >> 24) |
                        ((answer->TTL & 0x00FF0000) >>  8) |
                        ((answer->TTL & 0x0000FF00) <<  8) |
                        ((answer->TTL & 0x000000FF) << 24);
                }

                offset += sizeof(DNSAnswer);
                /* we want IPv4 internet addresses only */
                if (answer->TYPE != DNS_TYPE_A || answer->CLASS != DNS_CLASS_IN || answer->RDLENGTH != 4) {
                    offset += answer->RDLENGTH;
                    continue;
                }

                /* only support for ipv4 */
                memcpy(&ipv4, offset, sizeof(uint32_t));
                break;
            }
        }

        if(-1 != s) {
            lthread_close(s);
        }
        if(ipv4 != 0) {
            //got it
            break;
        }

        //retry the next
        lthread_sleep(500);
    }

#if 0
    if(0 == ipv4) {
        lthread_compute_begin();
        ipv4 = hostname_to_ip4_async(host, host_len);
        lthread_compute_end();
    }
#endif

OUT:
    if(NULL != sbuf) {
        free(sbuf);
    }
    if(NULL != rbuf) {
        free(rbuf);
    }

    //why - because EALREADY
    //lthread_sleep(0);

    return ipv4;
}

#if 0
//bug here, cannot use it
uint32_t hostname_to_ip4_async(const char * host, int host_len) {
    struct addrinfo hints, *servinfo = NULL, *p;
    struct sockaddr_in *h;
    int rv = 0;
    uint32_t ip = 0;
    char *hostname;

    if(host[host_len] != '\0') {
        hostname = (char*)malloc(host_len+1);
        memcpy(hostname, host, host_len);
        hostname[host_len] = '\0';
    } else {
        hostname = (char*)host;
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // AF_UNSPEC use AF_INET6 to force IPv6
    hints.ai_socktype = SOCK_STREAM;

    rv = getaddrinfo(hostname , "http" , &hints , &servinfo);

    if (rv != 0) {
        //fprintf(stderr, "getaddrinfo: error\n");
        goto err_dns;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        h = (struct sockaddr_in *) p->ai_addr;
        memcpy(&ip, &h->sin_addr, sizeof(uint32_t));
        break;
    }

err_dns:
    if(NULL != servinfo) {
        freeaddrinfo(servinfo); // all done with this structure
    }
    if(hostname != host) {
        free(hostname);
    }

    return ip;
}
#endif

#if 0
//bug here, cannot use it!
uint32_t hostname_to_ip4_async(const char * host, int host_len) {
    uint32_t ip4 = 0;
    int rc, rbuf_size = 2048;
    struct hostent hostinfo = {0}, *phost = NULL;
    char *dns_buff = malloc(rbuf_size);
    char *hostname;

    if(host[host_len] != '\0') {
        hostname = (char*)malloc(host_len+1);
        memcpy(hostname, host, host_len);
        hostname[host_len] = '\0';
    } else {
        hostname = (char*)host;
    }

    if (0 == gethostbyname_r(hostname, &hostinfo, dns_buff, rbuf_size, &phost, &rc)) {
        memcpy(&ip4, (uint32_t *)(hostinfo.h_addr_list[0]), sizeof(uint32_t));
    } else {
        ip4 = 0;
    }

    free(dns_buff);
    if(hostname != host) {
        free(hostname);
    }

    return ip4;
}
#endif

