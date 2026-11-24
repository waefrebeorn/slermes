#ifndef NOC_W20_SHAREDSESS_A7D92D_H
#define NOC_W20_SHAREDSESS_A7D92D_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/shared_session_attach.py (gate w20_sharedsess_a7d92d).
 *
 * Ported: the endpoint fence (`_local_origin`), the attach request the client
 * sends (`client_proof` + urlencoded query), the reply gate that ties a reply to
 * the registry's lease id (`discover_attach_url`'s proof/echo/endpoint checks)
 * and `configure_tui_attachment`'s transport precedence.
 *
 * Seams the fixture states: the registry snapshot (an owner list), the nonce
 * (`secrets.token_urlsafe(24)` is not reproducible), `session_owner_details`
 * (the message tail) and the HTTP handshake outcome.
 */

/* PoP: w20_sharedsess_a7d92d_local_origin @ hermes_cli/shared_session_attach.py:_local_origin
 * {"status":"ok","value":[host, port]} or
 * {"status":"error","type":"ValueError","message":...}. */
json_t *w20_sharedsess_a7d92d_local_origin(const char *url, const char *scheme);

/* PoP: w20_sharedsess_a7d92d_request @ hermes_cli/shared_session_attach.py:discover_attach_url
 * `client_proof = hex(HMAC_SHA256(lease_id, "hermes-session-attach-request:<sid>:<nonce>"))`
 * and `urlencode({"session_id","nonce","profile_home","client_proof"})`:
 *   {"status":"ok","value":{"client_proof":...,"query":...,"path":...}}. */
json_t *w20_sharedsess_a7d92d_request(const char *session_id, const char *nonce,
                                     const char *profile_home, const char *lease_id);

/* PoP: w20_sharedsess_a7d92d_verify @ hermes_cli/shared_session_attach.py:discover_attach_url
 * The reply gate: str websocket_url, echoed session_id/nonce/profile_home,
 * `attach_proof` compare_digest against the HMAC over the returned socket, and
 * `_local_origin(websocket_url, "ws") == origin` with path "/api/ws". */
json_t *w20_sharedsess_a7d92d_verify(const char *session_id, const char *nonce,
                                    const char *profile_home, const char *lease_id,
                                    const char *endpoint, const json_t *reply);

/* PoP: w20_sharedsess_a7d92d_discover @ hermes_cli/shared_session_attach.py:discover_attach_url
 * The whole decision over a scripted registry/handshake:
 *   owners      -- active_session_registry_snapshot(home, strict=True)
 *   handshake   -- {"reply": {...}} | {"error": {"type": ..., "message": ...}}
 *   owner_details -- session_owner_details(session_id, owner)
 * Answer: {"status":"ok","value":<websocket_url|null>} or an error object. */
json_t *w20_sharedsess_a7d92d_discover(const char *session_id, const json_t *owners,
                                      const char *nonce, const char *profile_home,
                                      const char *owner_details,
                                      const json_t *handshake);

/* PoP: w20_sharedsess_a7d92d_configure @ hermes_cli/shared_session_attach.py:configure_tui_attachment
 * An explicit HERMES_TUI_GATEWAY_URL (or no resumed session) wins; otherwise the
 * discovered URL is installed. Answer: the resulting env object. */
json_t *w20_sharedsess_a7d92d_configure(const json_t *env, const char *session_id,
                                       const json_t *discovered_url);

/* PoP: w20_sharedsess_a7d92d_attach @ hermes_cli/shared_session_attach.py:configure_tui_attachment
 * configure_tui_attachment's real call graph: an explicit transport or no session
 * short-circuits, otherwise discover_attach_url runs and its refusal propagates.
 * Answer: the resulting env object, or the discovery error object. */
json_t *w20_sharedsess_a7d92d_attach(const json_t *env, const char *session_id,
                                    const json_t *owners, const char *nonce,
                                    const char *profile_home, const char *owner_details,
                                    const json_t *handshake);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_SHAREDSESS_A7D92D_H */
