#ifndef NOC_W22_SESSIONPRO_F01E6E_H
#define NOC_W22_SESSIONPRO_F01E6E_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of plugins/platforms/matrix/session_proxy.py (gate w22_sessionpro_f01e6e).
 *
 * The module answers one question for a Matrix error: which proxy URL should
 * `describe_proxy_error` be handed for a session `_create_matrix_session`
 * built? The session's explicit proxy wins -- unless it is a SOCKS proxy and
 * `aiohttp_socks` is missing, in which case the session quietly ignored it and
 * the environment is what actually carried the request.
 *
 * Two seams travel as arguments instead of being read from the process:
 *   - `socks_connector_available`: is `import aiohttp_socks` satisfiable
 *     (`_socks_connector_missing()` is its negation),
 *   - `env`: the proxy environment the trust_env session would read, as an
 *     ordered {"NAME": "value"} object (the caller's os.environ).
 *
 * The env read itself is ported too (gateway.platforms.base_log_safety
 * .env_proxy_for_redaction): every `*_proxy` variable of any case and scheme
 * counts, the first value whose rendering cannot show it in full wins over the
 * first plain one, and NO_PROXY that bypasses every target means no proxy was
 * used at all (null).
 */

/* PoP: w22_sessionpro_f01e6e_socks_proxy @ plugins/platforms/matrix/session_proxy.py:matrix_redaction_proxy
 * `bool(proxy_url) and proxy_url.split("://")[0].lower().startswith("socks")`
 * -- 1 when the configured proxy is a SOCKS one (by scheme prefix). */
int w22_sessionpro_f01e6e_socks_proxy(const char *proxy_url);

/* PoP: w22_sessionpro_f01e6e_redaction_proxy @ plugins/platforms/matrix/session_proxy.py:matrix_redaction_proxy
 * The proxy URL to redact against: the explicit `proxy_url` when the session
 * dials it, else the environment value (or JSON null when there is none, when
 * trust_env is off, or when NO_PROXY bypasses the homeserver). */
json_t *w22_sessionpro_f01e6e_redaction_proxy(const char *proxy_url, const char *homeserver,
                                              int socks_connector_available, int trust_env,
                                              const json_t *env);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W22_SESSIONPRO_F01E6E_H */
