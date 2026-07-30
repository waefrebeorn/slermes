/*
 * web_server_ws_auth.h — WebSocket upgrade gate family (faithful C11 port
 * of the _ws_* auth cluster in hermes_cli/web_server.py).
 *
 * The Python functions read `app.state` + the Starlette WebSocket object;
 * the C port takes the same facts through explicit structs so the gate is
 * a pure decision layer over any WS transport.
 *
 * All char* results are malloc'd unless noted; caller frees.
 */
#ifndef WEB_SERVER_WS_AUTH_H
#define WEB_SERVER_WS_AUTH_H

#include <stdbool.h>

/* App-level state the Python code reads off app.state. */
typedef struct {
    bool auth_required;       /* OAuth gate active */
    const char *bound_host;   /* interface bound at listen time, may be NULL */
    const char *session_token; /* _SESSION_TOKEN, never NULL */
} ws_auth_state_t;

/* The WS-upgrade request facts the gates inspect. NULL == absent. */
typedef struct {
    const char *client_host;  /* ws.client.host; NULL/"" == no peer info */
    const char *host_header;  /* Host header */
    const char *origin_header;/* Origin header */
    const char *q_token;      /* ?token= */
    const char *q_ticket;     /* ?ticket= */
    const char *q_internal;   /* ?internal= */
} ws_upgrade_req_t;

/* _ws_client_reason: rejection token for the peer IP, or NULL when allowed. */
char *ws_auth_client_reason(const ws_auth_state_t *st,
                            const ws_upgrade_req_t *req);

/* _ws_client_is_allowed */
bool ws_auth_client_is_allowed(const ws_auth_state_t *st,
                               const ws_upgrade_req_t *req);

/* _ws_host_origin_reason: Host/Origin rejection token, or NULL. */
char *ws_auth_host_origin_reason(const ws_auth_state_t *st,
                                 const ws_upgrade_req_t *req);

/* _ws_host_origin_is_allowed */
bool ws_auth_host_origin_is_allowed(const ws_auth_state_t *st,
                                    const ws_upgrade_req_t *req);

/* _ws_request_reason: first Host/Origin or peer-IP reason, or NULL. */
char *ws_auth_request_reason(const ws_auth_state_t *st,
                             const ws_upgrade_req_t *req);

/* _ws_request_is_allowed */
bool ws_auth_request_is_allowed(const ws_auth_state_t *st,
                                const ws_upgrade_req_t *req);

/* _ws_auth_mode: "gated" | "insecure" | "loopback" (static string). */
const char *ws_auth_mode(const ws_auth_state_t *st);

/* _ws_auth_reason: validates the WS-upgrade credential.
 * On return, *credential is a static string naming the credential type
 * ("ticket" | "internal" | "token" | "none"). Returns NULL when accepted,
 * else a static rejection token ("no_credential" | "token_mismatch" |
 * "ticket_invalid" | "internal_invalid"). */
const char *ws_auth_reason(const ws_auth_state_t *st,
                           const ws_upgrade_req_t *req,
                           const char **credential);

/* _ws_auth_ok */
bool ws_auth_ok(const ws_auth_state_t *st, const ws_upgrade_req_t *req);

/* _has_valid_query_token: narrow query-token allowlist for download links. */
bool ws_auth_has_valid_query_token(const ws_auth_state_t *st,
                                   const char *path, const char *q_token);

#endif /* WEB_SERVER_WS_AUTH_H */
