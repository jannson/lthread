#ifndef __CDNS_H__
#define __CDNS_H__

int cdns_init(void);
void cdns_deinit(void);
void add_resolver(const char * resolver);
uint32_t dns_lookup(const char * host, int host_len);
//uint32_t hostname_to_ip4_async(const char * host, int host_len);

#endif

