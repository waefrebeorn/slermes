#ifndef NOC_W5_INIT_7D1849_H
#define NOC_W5_INIT_7D1849_H

#include <stddef.h>
struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w5_init_7d1849_is_truthy @ utils/__init__.py:is_truthy_value */
int w5_init_7d1849_is_truthy(const struct json_t *value, int default_value);

/* PoP: w5_init_7d1849_base_url_hostname @ utils/__init__.py:base_url_hostname */
char *w5_init_7d1849_base_url_hostname(const char *base_url);

/* PoP: w5_init_7d1849_base_url_path @ utils/__init__.py:base_url_path */
char *w5_init_7d1849_base_url_path(const char *base_url);

/* PoP: w5_init_7d1849_base_url_origin @ utils/__init__.py:base_url_origin
 * JSON array [scheme, hostname, effective_port]. */
struct json_t *w5_init_7d1849_base_url_origin(const char *base_url);

/* PoP: w5_init_7d1849_same_origin @ utils/__init__.py:same_base_url_origin */
int w5_init_7d1849_same_origin(const char *a, const char *b);

/* PoP: w5_init_7d1849_host_matches @ utils/__init__.py:base_url_host_matches */
int w5_init_7d1849_host_matches(const char *base_url, const char *domain);

/* PoP: w5_init_7d1849_normalize_proxy @ utils/__init__.py:normalize_proxy_url
 * NULL output means Python None. Caller frees non-NULL. */
char *w5_init_7d1849_normalize_proxy(const char *url);

/* PoP: w5_init_7d1849_env_int @ utils/__init__.py:env_int
 * `raw` is the injected environment value; decimal strings carry 64-bit ints. */
struct json_t *w5_init_7d1849_env_int(const char *raw, const char *decimal_default);

/* PoP: w5_init_7d1849_env_float @ utils/__init__.py:env_float
 * `raw` is the injected environment value. */
struct json_t *w5_init_7d1849_env_float(const char *raw, const char *double_default);

/* PoP: w5_init_7d1849_sanitize_no_proxy @ utils/__init__.py:sanitize_no_proxy_entries */
char *w5_init_7d1849_sanitize_no_proxy(const char *value);

/* PoP: w5_init_7d1849_all @ utils/__init__.py:__all__ */
struct json_t *w5_init_7d1849_all(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W5_INIT_7D1849_H */
