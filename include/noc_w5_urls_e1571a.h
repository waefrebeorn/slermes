#ifndef NOC_W5_URLS_E1571A_H
#define NOC_W5_URLS_E1571A_H
#include <stddef.h>

/* PoP: w5urls_hostname @ utils/urls.py:base_url_hostname */
int w5urls_hostname(const char *base_url, char *out, size_t cap);
/* PoP: w5urls_path @ utils/urls.py:base_url_path */
int w5urls_path(const char *base_url, char *out, size_t cap);
/* PoP: w5urls_origin @ utils/urls.py:base_url_origin
 * Returns 1 when a usable origin exists (fills scheme/host/port), 0 for
 * Python's ("", "", 0) sentinel (no host or invalid port). */
int w5urls_origin(const char *base_url, char *scheme, size_t scap,
                  char *host, size_t hcap, int *port);
/* PoP: w5urls_same_origin @ utils/urls.py:same_base_url_origin */
int w5urls_same_origin(const char *a, const char *b);
/* PoP: w5urls_host_matches @ utils/urls.py:base_url_host_matches */
int w5urls_host_matches(const char *base_url, const char *domain);

#endif /* NOC_W5_URLS_E1571A_H */
