/* Public C API for the agent/fallback_notice.py parity port. */
#ifndef NOC_W7_FALLBACKNOT_A6D13B_H
#define NOC_W7_FALLBACKNOT_A6D13B_H
#include "hermes_json.h"

json_t *w7_fallbacknot_a6d13b_constants(void);
/* reason: reason-name string or null. */
json_t *w7_fallbacknot_a6d13b_reason_text(json_t *reason);
/* stashed: null or {"reason": <name-or-null>, "detail": <string-or-null>}. */
json_t *w7_fallbacknot_a6d13b_trigger_text(json_t *reason, json_t *stashed);
json_t *w7_fallbacknot_a6d13b_notice_hidden(json_t *display_kind);
#endif /* NOC_W7_FALLBACKNOT_EC1211_H */
