#ifndef NOC_W22_HTTPCLIENT_5F808A_H
#define NOC_W22_HTTPCLIENT_5F808A_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of gateway/platforms/_http_client_limits.py (gate w22_httpclient_5f808a).
 *
 * `_positive_env` and the `httpx.Limits` it feeds are pure parsers of two env
 * values. The env read itself is the seam: the caller hands over the raw
 * `os.environ.get(name, "")` text, so nothing here touches the process
 * environment (and Windows `setenv(x, "")` unsetting the variable cannot bite).
 * `httpx` being importable is the second seam (`have_httpx`): without it
 * `platform_httpx_limits()` is None.
 */

/* PoP: w22_httpclient_5f808a_positive_env_int @ gateway/platforms/_http_client_limits.py:_positive_env
 * `int(raw)` when set, parseable and > 0 (>= 0 when *allow_zero*); else *default*.
 * Python's int() spelling: surrounding whitespace, one sign, ASCII digits with
 * single underscores between them. Values beyond int64 read as unparseable
 * (Python's integer is unbounded; the C ABI carries int64). */
long long w22_httpclient_5f808a_positive_env_int(const char *raw, long long def, int allow_zero);

/* PoP: w22_httpclient_5f808a_positive_env_float @ gateway/platforms/_http_client_limits.py:_positive_env
 * `float(raw)` when set, parseable and > 0; else *default*. Same whitespace and
 * underscore rules; `inf`/`nan` parse but never pass the `> 0` gate. */
double w22_httpclient_5f808a_positive_env_float(const char *raw, double def);

/* PoP: w22_httpclient_5f808a_limits @ gateway/platforms/_http_client_limits.py:platform_httpx_limits
 * `{"max_keepalive_connections": int, "keepalive_expiry": float}`, or null when
 * httpx is not importable. Defaults: 10 keepalive connections, 2.0s expiry;
 * `HERMES_GATEWAY_HTTPX_MAX_KEEPALIVE=0` disables keepalive. */
json_t *w22_httpclient_5f808a_limits(const char *raw_max_keepalive, const char *raw_expiry,
                                     int have_httpx);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_HTTPCLIENT_5F808A_H */
