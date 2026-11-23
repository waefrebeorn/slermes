#ifndef NOC_W13_USERMESSAG_547811_H
#define NOC_W13_USERMESSAG_547811_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* C11 port of tui_gateway/user_messages.py (gate w13_usermessag_547811).
 * All returned strings are heap allocated and owned by the caller (free()).
 * `surface` args are JSON objects (or NULL for Python None / non-dict).
 * `recoverable` / `compressing` are 0/1 booleans. */

char *w13_usermessag_547811_identity(const struct json_t *surface);
char *w13_usermessag_547811_turn_error_title(const struct json_t *surface);
char *w13_usermessag_547811_turn_error_hint(const struct json_t *surface, int recoverable);
char *w13_usermessag_547811_turn_error_text(const char *error, const struct json_t *surface, int recoverable);
char *w13_usermessag_547811_busy_message(const char *command, int compressing);
char *w13_usermessag_547811_agent_init_failed_message(const char *exc);
char *w13_usermessag_547811_resume_failed_message(const char *exc);
char *w13_usermessag_547811_agent_still_starting(void);
char *w13_usermessag_547811_agent_build_abandoned(void);
char *w13_usermessag_547811_agent_missing_for_turn(void);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W13_USERMESSAG_547811_H */
