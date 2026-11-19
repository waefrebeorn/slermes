/*
 * port_conversation_compression.h -- pure helpers ported from
 * agent/conversation_compression.py.
 */
#ifndef PORT_CONVERSATION_COMPRESSION_H
#define PORT_CONVERSATION_COMPRESSION_H

#include "libjson/json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _strip_marker_for_comparison -- copy `msgs` with the ``_db_persisted``
 * marker dropped from every dict entry. A non-list passes through unchanged;
 * so does a non-dict entry. Caller owns the result. */
json_t *cc_strip_marker_for_comparison(const json_t *msgs);

/* is_compaction_progress_status -- True for in-progress auto-compaction
 * lifecycle lines, False for the done edge, idle/preflight/retry and the
 * blocked-overflow notice. */
int cc_is_compaction_progress_status(const char *text, int marked);

#ifdef __cplusplus
}
#endif

#endif /* PORT_CONVERSATION_COMPRESSION_H */
