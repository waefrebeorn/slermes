#ifndef NOC_W20_TCPSITE_D7FD64_H
#define NOC_W20_TCPSITE_D7FD64_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of gateway/platforms/tcp_site.py (gate w20_tcpsite_d7fd64).
 *
 * Ported: `has_live_listener`'s probe classification and `start_tcp_site`'s
 * bind/retry decision (macOS exclusivity, EADDRINUSE, wildcard hosts, the
 * TIME_WAIT retry with SO_REUSEADDR). The socket and the aiohttp runner stay
 * upstream; the fixture states the platform, the probe answer and the errno.
 */

/* PoP: w20_tcpsite_d7fd64_has_live_listener @ gateway/platforms/tcp_site.py:has_live_listener
 * "ok" -> 1 (something accepts), "refused" -> 0 (nobody listens), anything else
 * (timeout, unroutable) -> 1 so the caller stays exclusive. */
int w20_tcpsite_d7fd64_has_live_listener(const char *probe);

/* PoP: w20_tcpsite_d7fd64_is_wildcard_host @ gateway/platforms/shared_ingress.py:is_wildcard_host
 * None / "" / "0.0.0.0" / "::" / "*" (after strip) are the wildcards. */
int w20_tcpsite_d7fd64_is_wildcard_host(const char *host);

/* PoP: w20_tcpsite_d7fd64_start @ gateway/platforms/tcp_site.py:start_tcp_site
 *   platform        -- sys.platform ("darwin" makes the bind exclusive)
 *   first/second    -- the errno name each site.start() raises, or "ok"
 *   listener_live   -- has_live_listener(host, port)
 * Answer: {"calls":[{"reuse":null|false|true,"result":...}], "stops":N,
 *          "log":"..."|null, "outcome":"site"} or
 *         {"status":"raised","type":"OSError","errno":"EADDRINUSE"}. */
json_t *w20_tcpsite_d7fd64_start(const char *platform, const char *host, int port,
                                 const char *log_tag, const char *first,
                                 int listener_live, const char *second);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_TCPSITE_D7FD64_H */
