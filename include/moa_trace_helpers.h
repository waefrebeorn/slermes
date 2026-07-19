/*
 * moa_trace_helpers.h — public API for the pure agent/moa_trace.py helpers.
 * Opaque, minimal includes.
 */

#ifndef MOA_TRACE_HELPERS_H
#define MOA_TRACE_HELPERS_H

#include <stddef.h>

/* Make a session id safe as a filename component (keep alnum/-_., replace
 * others with '_'; empty -> "unknown-session"). Caller frees.
 * (PoP: _sanitize_session_id) */
char *moa_trace_sanitize_session_id(const char *session_id);

#endif /* MOA_TRACE_HELPERS_H */
