/*
 * noc_w29_adapter_0e0bda_p1.h - C surface for upstream
 * plugins/platforms/a2a/adapter.py, gate p1 (module-level value helpers).
 *
 * The adapter's inbound HTTP surface is a fixture seam here: no socket is
 * opened and no thread started.  What this gate owns is the value arithmetic
 * the handler threads run before and around the socket --
 *   * the reply/orphan timeout budgets (A2A_REPLY_TIMEOUT parsing + clamping),
 *   * the URL-safe slug/context-slug sanitisers,
 *   * the public-URL joiner peers behind a reverse proxy are advertised,
 *   * the family/peer-address predicates, and
 *   * the module budget constants themselves.
 */
#ifndef NOC_W29_ADAPTER_0E0BDA_P1_H
#define NOC_W29_ADAPTER_0E0BDA_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The module budget constants, as an object. */
json_t *w29_adapter_0e0bda_constants(void);

/* env_value: the A2A_REPLY_TIMEOUT string as captured from the profile scope
 * (null => unset) -> seconds to wait for the agent to answer, >= 1.0. */
json_t *w29_adapter_0e0bda_reply_timeout(const json_t *env_value);

/* env_value: the same A2A_REPLY_TIMEOUT capture -> the orphan grace,
 * clamped into [_MIN_ORPHAN_TIMEOUT, _MAX_ORPHAN_TIMEOUT]. */
json_t *w29_adapter_0e0bda_orphan_timeout(const json_t *env_value);

/* value: any JSON value -> the single-segment served-agent slug ("" for the
 * root agent, "default" and "root"). */
json_t *w29_adapter_0e0bda_clean_slug(const json_t *value);

/* base, prefix -> the URL a peer must call back (urljoin semantics). */
json_t *w29_adapter_0e0bda_join_url(const json_t *base, const json_t *prefix);

/* value, max_len (null => 96) -> the attacker-safe context slug used in
 * session titles. */
json_t *w29_adapter_0e0bda_safe_context_slug(const json_t *value, const json_t *max_len);

/* host -> "v6" when the listener needs the AF_INET6 family, else "v4". */
json_t *w29_adapter_0e0bda_server_class(const json_t *host);

/* addr: the socket peer address -> the peer's plain identity ("::ffff:a.b.c.d"
 * collapses to "a.b.c.d"). */
json_t *w29_adapter_0e0bda_client_ip(const json_t *addr);

#ifdef __cplusplus
}
#endif
#endif
