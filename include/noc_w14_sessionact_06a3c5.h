#ifndef NOC_W14_SESSIONACT_06A3C5_H
#define NOC_W14_SESSIONACT_06A3C5_H

#include "hermes_json.h"

/* C port of gateway/session_active_list.py (gate w14_sessionact_06a3c5).
 *
 * The live module reads the session authority and event stream; all of that
 * state is injected as explicit JSON args here so the row projection replays
 * deterministically. INTERNAL_LISTING_SOURCES is ('kanban','tool','oneshot').
 *
 * status -> new json_t* string: "waiting" | "working" | "starting" | "idle".
 * active_list -> new json_t*:
 *   {"ok": bool, "error": <msg>|null, "sessions": <array>|null}
 * On failure error is "invalid_params" or "permission_denied".
 */
json_t *w14_sessionact_06a3c5_status(const char *execution_state, int asked,
                                     json_t *pending);
json_t *w14_sessionact_06a3c5_active_list(json_t *params,
                                          json_t *capabilities,
                                          json_t *entries);

#endif /* NOC_W14_SESSIONACT_06A3C5_H */
