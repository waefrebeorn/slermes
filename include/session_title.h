/* session_title.h — session title plumbing, faithful C11 port of the title
 * surface of hermes_state.py (SessionDB.sanitize_title, get_session_title,
 * set_session_title, set_auto_title_if_empty, get_next_title_in_lineage).
 *
 * Backed by libdb (lib/libdb) — the C tree's canonical session store —
 * via its opaque db_t handle and sidecar session metadata. Uniqueness is
 * enforced across the store like the Python sqlite unique-title index.
 * Implemented in src/cli/port_session_title.c.
 */

#ifndef SLERMES_SESSION_TITLE_H
#define SLERMES_SESSION_TITLE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct db_t db_t;   /* opaque, from lib/libdb */

/* Result codes mirroring the Python contract:
 * OK        -> True (written)
 * SKIPPED   -> predicate failed (only_if_empty and a title existed) / no-op
 * NOT_FOUND -> session missing (Python set_session_title returns False)
 * CONFLICT  -> title in use by another session (Python raises ValueError)
 * INVALID   -> title failed validation (too long — Python ValueError) */
typedef enum {
    SESSION_TITLE_OK = 0,
    SESSION_TITLE_SKIPPED = 1,
    SESSION_TITLE_NOT_FOUND = 2,
    SESSION_TITLE_CONFLICT = 3,
    SESSION_TITLE_INVALID = 4,
} session_title_result_t;

/* Validate and sanitize a session title (pure).
 * Strips, removes ASCII + problematic Unicode control chars, collapses
 * whitespace runs, normalizes empty -> NULL. Returns a fresh string or NULL
 * (caller frees). Sets *invalid=true when the cleaned title exceeds 100
 * code points (Python raises ValueError). */
char *session_title_sanitize(const char *title, bool *invalid);

/* Get the title for a session, or NULL (caller frees). */
char *session_title_get(db_t *db, const char *session_id);

/* Set or update a session's title (uniqueness-checked). */
session_title_result_t session_title_set(db_t *db, const char *session_id,
                                         const char *title);

/* Set an auto-generated title only when the current title is empty —
 * predicate + write against the same store so a manual rename wins. */
session_title_result_t session_title_set_auto_if_empty(db_t *db,
                                                        const char *session_id,
                                                        const char *title);

/* Generate the next title in a lineage ("my session" -> "my session #2").
 * Scans existing titles in the store. Returns a fresh string (caller frees). */
char *session_title_next_in_lineage(db_t *db, const char *base_title);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_SESSION_TITLE_H */
