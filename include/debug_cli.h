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
#include "hermes_json.h"

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

/* ── debug-share core (hermes_cli/debug.py) ──────────────────────────────
 * Real implementations of the share pipeline. Network uploads go through
 * libhttp; pending-delete bookkeeping reuses this header's store above. */

/* Single-read snapshot of one log file (LogSnapshot). */
typedef struct {
    char *tail_text;  /* summary tail for the report */
    char *full_text;  /* full text for the standalone paste; NULL if none */
} debug_log_snapshot_t;

/* Capture a log once and derive tail/full views from the same read.
 * Mirrors _capture_log_snapshot: resolves primary or .1 rotation, reads at
 * most ~512KB from the end, optionally force-redacts both views. */
void debug_capture_log_snapshot(const char *log_name, int tail_lines,
                                int redact, debug_log_snapshot_t *out);

/* Free a snapshot's strings. */
void debug_free_log_snapshot(debug_log_snapshot_t *snap);

/* collect_debug_report @ hermes_cli/debug.py:collect_debug_report — build the
 * summary report: system dump + per-log tails, exactly Python's layout.
 * Any tail may be NULL/empty. Returns a malloc'd string. */
char *debug_collect_debug_report(int log_lines, const char *dump_text,
                                 const char *agent_tail,
                                 const char *errors_tail,
                                 const char *gateway_tail,
                                 const char *gui_tail,
                                 const char *desktop_tail);

/* Upload content to a paste service (paste.rs first, dpaste.com fallback).
 * Returns a malloc'd URL, or NULL with *err set to a malloc'd message
 * mirroring the RuntimeError text. */
char *debug_upload_to_pastebin(const char *content, int expiry_days,
                               char **err);

/* build_debug_share @ hermes_cli/debug.py:build_debug_share — collect report +
 * full logs, upload each, schedule auto-deletion. Returns a JSON object:
 *   {"urls": {label: url}, "failures": ["label: err"],
 *    "redacted": bool, "auto_delete_seconds": 21600, "report": "..."}
 * NULL (with *err) when the required report upload fails. */
json_t *debug_build_debug_share(int log_lines, int expiry_days, int redact,
                                char **err);

/* run_debug_share @ hermes_cli/debug.py:run_debug_share — the CLI flow.
 * `confirm_cb` returns nonzero to proceed with upload (interactive consent
 * gate; pass NULL together with confirm=1 to skip prompting). Returns process
 * exit code (0 success, 1 failure/abort). */
int debug_run_debug_share(int lines, int expire, int local_only, int nous,
                          int redact,
                          const char **extra_argv, int extra_argc);

/* run_debug_delete @ hermes_cli/debug.py:run_debug_delete — delete pastes by
 * URL. Returns process exit code (0). */
int debug_run_debug_delete(const char *const *urls, int n);

#ifdef __cplusplus
}
#endif

#endif /* DEBUG_CLI_H */
