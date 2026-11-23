#ifndef NOC_W5_ENV_EE14B1_H
#define NOC_W5_ENV_EE14B1_H
#include <stdbool.h>
#include <stddef.h>

/* C home for upstream Hermes utils/env.py (gate w5_env_ee14b1).
 * Pure value coercion and proxy-URL normalization are ported faithfully.
 * The env-reading helpers expose an explicit _ex seam: the value is injected
 * from the fixture instead of mutating the process environment (Windows CRT
 * setenv(name,"",1) UNSETS the variable, while Python os.environ[name]="")
 * keeps it). */

typedef enum {
    W5_ENV_EE14B1_NULL = 0,
    W5_ENV_EE14B1_STRING = 1,
    W5_ENV_EE14B1_NUMBER = 2,
    W5_ENV_EE14B1_BOOL = 3
} w5_env_ee14b1_vkind_t;

/* PoP: w5_env_ee14b1_is_truthy_value @ utils/env.py:is_truthy_value */
bool w5_env_ee14b1_is_truthy_value(w5_env_ee14b1_vkind_t kind,
                                   const char *str, double num, bool b, bool def);

/* PoP: w5_env_ee14b1_normalize_proxy_url @ utils/env.py:normalize_proxy_url
 * Returns a malloc'd string, or NULL for Python None. */
char *w5_env_ee14b1_normalize_proxy_url(const char *proxy_url);

/* PoP: w5_env_ee14b1_bare_ipv6_literal @ utils/env.py:_bare_ipv6_literal */
char *w5_env_ee14b1_bare_ipv6_literal(const char *entry);

/* PoP: w5_env_ee14b1_sanitize_no_proxy_entries @ utils/env.py:sanitize_no_proxy_entries */
char *w5_env_ee14b1_sanitize_no_proxy_entries(const char *no_proxy_value);

/* PoP: w5_env_ee14b1_env_int_ex @ utils/env.py:env_int / _env_number
 * raw == NULL means the variable is unset; "" means present-but-empty. */
long long w5_env_ee14b1_env_int_ex(const char *raw, long long def);

/* PoP: w5_env_ee14b1_env_float_ex @ utils/env.py:env_float / _env_number */
double w5_env_ee14b1_env_float_ex(const char *raw, double def);

/* PoP: w5_env_ee14b1_free @ utils/env.py:<module> */
void w5_env_ee14b1_free(char *s);

#endif /* NOC_W5_ENV_EE14B1_H */
