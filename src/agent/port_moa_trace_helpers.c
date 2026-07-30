/*
 * port_moa_trace_helpers.c — pure helpers ported from agent/moa_trace.py.
 * Self-contained, no IO/runtime deps beyond string ops.
 *
 *   - _sanitize_session_id -> moa_trace_sanitize_session_id
 */

#include "moa_trace_helpers.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* PoP: _sanitize_session_id @ agent/moa_trace.py:_sanitize_session_id */
/* Make a session id safe as a filename component: keep alphanumerics and
 * '-', '_', '.', replace every other char with '_'. Empty/None -> "unknown-session".
 * Result malloc'd; caller frees. */
/* PoP: moa_trace_sanitize_session_id @ agent/moa_trace.py:_sanitize_session_id */
char *moa_trace_sanitize_session_id(const char *session_id)
{
    if (!session_id || !*session_id)
        return strdup("unknown-session");

    size_t n = strlen(session_id);
    char *out = malloc(n + 1);
    size_t j = 0;
    for (size_t i = 0; i < n; i++) {
        char c = session_id[i];
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.')
            out[j++] = c;
        else
            out[j++] = '_';
    }
    out[j] = '\0';
    return out;
}
