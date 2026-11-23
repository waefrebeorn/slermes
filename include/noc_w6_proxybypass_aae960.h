#ifndef NOC_W6_PROXYBYPASS_AAE960_H
#define NOC_W6_PROXYBYPASS_AAE960_H

/*
 * Public C API for the port of agent/proxy_bypass.py (gate w6_proxybypass_aae960).
 * All exported symbols are proxybypass_-prefixed. Pure leaf: stdlib only.
 */
#include "hermes_json.h"

/* split_host_port(value) -> {"host":str,"port":int|null}. */
json_t *proxybypass_split_host_port(const char *value);

/* no_proxy_entries(no_proxy_value) -> array of str. */
json_t *proxybypass_no_proxy_entries(const char *value);

/* is_loopback_host(host) -> bool. */
json_t *proxybypass_is_loopback(const char *host);

/* no_proxy_entry_matches(entry, host, has_port, port) -> bool. */
json_t *proxybypass_entry_matches(const char *entry, const char *host,
                                  int has_port, double port);

/* should_bypass_proxy(targets (array of str), no_proxy_value) -> bool. */
json_t *proxybypass_should_bypass(const json_t *targets, const char *no_proxy_value);

/* loopback_connect_kwargs(url) -> {"proxy":null} or {}. */
json_t *proxybypass_connect_kwargs(const char *url);

/* loopback_request_kwargs(url) -> {"proxies":{"http":null,"https":null}} or {}. */
json_t *proxybypass_request_kwargs(const char *url);

#endif /* NOC_W6_PROXYBYPASS_AAE960_H */
