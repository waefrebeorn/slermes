/*
 * debug_cli.h — `hermes debug` helpers (faithful C11 port of
 * hermes_cli/debug.py pure logic).
 *
 * Ports the self-contained, testable core:
 *   - paste.rs URL -> paste-id extraction
 *   - pending-deletion tracking store (~/.hermes/pastes/pending.json):
 *     load / save / record / sweep (deterministic given a clock + delete cb)
 *   - log-text redaction (reuses hermes_redact + email masking)
 *
 * Network upload (paste.rs / dpaste / Nous S3) is intentionally NOT ported
 * here; the store + expiry logic is the part worth unit-testing.
 */

#ifndef DEBUG_CLI_H
#define DEBUG_CLI_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Extract a paste.rs paste ID from a URL, or NULL if not a known service.
 * Caller frees. */
char *debug_extract_paste_id(const char *url);

/* Pending-deletion entry. */
typedef struct {
    char *url;
    double expire_at; /* unix seconds */
} debug_pending_t;

/* Path to <hermes_home>/pastes/pending.json (caller-owned copy). */
char *debug_pending_path(const char *hermes_home);

/* Load well-formed entries from pending.json. Caller frees array with
 * debug_free_pending(). Returns count (0 if missing/malformed). */
debug_pending_t *debug_load_pending(const char *hermes_home, int *out_count);

/* Persist entries to pending.json (atomic-ish: write temp + rename). */
bool debug_save_pending(const char *hermes_home, debug_pending_t *entries, int n);

/* Record urls for deletion at now+delay_seconds. Only paste.rs URLs (those
 * with an extractable id) are recorded; merged by URL keeping the later
 * expire_at. */
void debug_record_pending(const char *hermes_home, const char **urls, int n,
                          double now, int delay_seconds);

/* Sweep expired pastes. delete_cb(url) -> 1 if deleted, 0 if failed (may be
 * NULL for dry-run). Failed deletes are retained for up to 24h past
 * expiration, then reaped. Sets *out_deleted / *out_remaining. */
void debug_sweep_expired_pastes(const char *hermes_home, double now,
                                int (*delete_cb)(const char *url),
                                int *out_deleted, int *out_remaining);

void debug_free_pending(debug_pending_t *arr, int n);

/* Redact log text headed for a public paste: force-redact secrets (reuses
 * hermes_redact) then mask email addresses. Caller frees. Returns a copy
 * (or NULL if input empty). */
char *debug_redact_log_text(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* DEBUG_CLI_H */
