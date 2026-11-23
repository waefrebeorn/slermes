#ifndef NOC_W5_RUNAPI_7D29D9_H
#define NOC_W5_RUNAPI_7D29D9_H

/*
 * Public C API for the port of gateway/run_api.py (gate w5_runapi_7d29d9).
 * All exported symbols are runapi_-prefixed to avoid collisions.
 *
 * The pure listener decisions are ported with their runtime facts resolved by
 * the caller (networking/async side effects are not part of these leaves).
 */
#include "hermes_json.h"

/* Port of _listener_public_url(webapp): NULL when no webapp, else its
 * public_url. Returns json string|null (caller frees). */
json_t *runapi_listener_public_url(int webapp_present, const json_t *public_url);

/* Port of _embedded_chat_enabled(): True when the web server module is not
 * loaded; else its _DASHBOARD_EMBEDDED_CHAT_ENABLED flag (default True).
 * Returns json bool (caller frees). */
json_t *runapi_embedded_chat_enabled(int loaded_web, int has_flag, int flag_val);

/* Port of the setsockopt decision in _listener_socket(): which of
 * IPV6_V6ONLY=0, SO_REUSEADDR=1, SO_EXCLUSIVEADDRUSE=1 are applied.
 * Returns {"dual_stack":bool,"reuseaddr":bool,"exclusive":bool} (caller frees). */
json_t *runapi_socket_options(double port, int is_nt, int dual_stack);

#endif /* NOC_W5_RUN_API_7D29D9_H */
