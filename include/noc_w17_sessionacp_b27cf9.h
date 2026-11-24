/*
 * noc_w17_sessionacp_b27cf9.h — public C API for the faithful C11 port of
 * upstream Hermes gateway/session_acp_lifecycle.py (gate w17_sessionacp_b27cf9).
 */
#ifndef NOC_W17_SESSIONACP_B27CF9_H
#define NOC_W17_SESSIONACP_B27CF9_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w17_sessionacp_b27cf9_end_idle_acp_session @
 *      gateway/session_acp_lifecycle.py:end_idle_acp_session */
json_t *w17_sessionacp_b27cf9_end_idle_acp_session(json_t *authority, const char *session_id,
        json_t *io, json_t *state);

/* PoP: w17_sessionacp_b27cf9_end_idle_viewer_session @
 *      gateway/session_acp_lifecycle.py:end_idle_viewer_session */
json_t *w17_sessionacp_b27cf9_end_idle_viewer_session(json_t *authority, const char *session_id,
        const char *reason, json_t *io, json_t *state);

/* PoP: w17_sessionacp_b27cf9_reopen_if_reattached @
 *      gateway/session_acp_lifecycle.py:reopen_if_reattached */
json_t *w17_sessionacp_b27cf9_reopen_if_reattached(json_t *authority, const char *session_id,
        bool ended, json_t *io, json_t *state);

/* PoP: w17_sessionacp_b27cf9_end_idle_acp_sessions @
 *      gateway/session_acp_lifecycle.py:end_idle_acp_sessions */
json_t *w17_sessionacp_b27cf9_end_idle_acp_sessions(json_t *authority, json_t *io, json_t *state);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W17_SESSIONACP_B27CF9_H */
