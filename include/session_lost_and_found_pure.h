#ifndef HERMES_SESSION_LOST_AND_FOUND_PURE_H
#define HERMES_SESSION_LOST_AND_FOUND_PURE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Pure, deterministic classification heuristics ported from
 * hermes_cli/session_lost_and_found.py. laf_classify_row returns a malloc'd
 * JSON string ("sessions" | "messages" | "session_model_usage" | "null"). */

int   laf_is_session_id(const char *value);
int   laf_looks_like_source(const char *value);
char *laf_classify_row(int nfield, const char *cells_json);
double laf_heuristic_started_at(const char *cells_json);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_SESSION_LOST_AND_FOUND_PURE_H */
