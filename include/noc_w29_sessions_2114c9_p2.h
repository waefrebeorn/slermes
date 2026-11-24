/*
 * noc_w29_sessions_2114c9_p2.h - C surface for upstream
 * persistence/sqlite/conversation/sessions.py (gate w29_sessions_2114c9_p2).
 *
 * Gate p2: session_record - the whole legacy row (get_session and the listing
 * rows) mapped to the SessionRecord contract: lineage markers, the degraded
 * instants, the tolerant model_config, the title-source vocabulary and the
 * tool pin.  Together with the constant tables that shaping consults.
 */
#ifndef NOC_W29_SESSIONS_2114C9_P2_H
#define NOC_W29_SESSIONS_2114C9_P2_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req {row} -> the SessionRecord fields as an object  (sessions.py:session_record) */
json_t *w29_sessions_2114c9_p2_session_record(const json_t *req);

/* -> markers, title_sources, canonical-title text and the instant window  (sessions.py constants) */
json_t *w29_sessions_2114c9_p2_constants(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
