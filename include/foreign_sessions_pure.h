#ifndef HERMES_FOREIGN_SESSIONS_PURE_H
#define HERMES_FOREIGN_SESSIONS_PURE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Pure, deterministic helpers ported from hermes_cli/foreign_sessions.py.
 * All return malloc'd strings (free with free()). */

int   foreign_sessions_is_wrapper_text(const char *text);
char *foreign_sessions_flatten_blocks(const char *content_json, const char *source);
char *foreign_sessions_merge_turns(const char *raw_turns_json);
char *foreign_sessions_session_label(const char *source, const char *title_guess,
                                     const char *stem);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_FOREIGN_SESSIONS_PURE_H */
