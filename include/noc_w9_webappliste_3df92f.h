#ifndef NOC_W9_WEBAPPLISTE_3DF92F_H
#define NOC_W9_WEBAPPLISTE_3DF92F_H
#include "hermes_json.h"

/* PoP port of gateway/webapp_listener.py — host / URL address helpers. */
char   *w9_webappliste_3df92f_bare_host(const char *host);
int     w9_webappliste_3df92f_is_wildcard(const char *host);
int     w9_webappliste_3df92f_is_loopback(const char *host);
char   *w9_webappliste_3df92f_dial_host(const char *host);
char   *w9_webappliste_3df92f_dial_origin(const char *host, const char *port);
json_t *w9_webappliste_3df92f_public_url(json_t *value);
#endif /* NOC_W9_WEBAPPLISTE_3DF92F_H */
