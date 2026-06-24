/*
 * helpers.h — Shared gateway helper utilities.
 * Port of Python gateway/platforms/helpers.py.
 *
 * Provides: message deduplication, markdown stripping, thread
 * participation tracking, phone number redaction.
 */

#ifndef GATEWAY_HELPERS_H
#define GATEWAY_HELPERS_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Message Deduplicator — TTL-based message ID dedup cache
 * ================================================================ */

typedef struct {
    char   **msg_ids;       /* array of message ID strings */
    double *timestamps;     /* array of timestamps */
    int     count;          /* current count */
    int     max_size;       /* max entries (default 2000) */
    double  ttl_seconds;    /* TTL in seconds (default 300) */
} msg_dedup_t;

/* Initialize a deduplicator with defaults (max_size=2000, ttl=300). */
void msg_dedup_init(msg_dedup_t *d);

/* Initialize with custom parameters. */
void msg_dedup_init_custom(msg_dedup_t *d, int max_size, double ttl_seconds);

/* Check if msg_id is a duplicate within the TTL window.
 * Returns true if already seen (and updates timestamp).
 * Returns false if not seen (registers it). */
bool msg_dedup_is_duplicate(msg_dedup_t *d, const char *msg_id);

/* Clear all tracked messages. */
void msg_dedup_clear(msg_dedup_t *d);

/* Free internal resources. Does NOT free the struct itself. */
void msg_dedup_destroy(msg_dedup_t *d);

/* ================================================================
 *  Markdown Stripping — strip formatting for plain-text platforms
 * ================================================================ */

/* Strip markdown formatting from text.
 * Returns a malloc'd string (caller must free).
 * Handles: **bold**, *italic*, __bold__, _italic_, ```code blocks```,
 * `inline code`, # headings, [links](url), and collapses 3+ newlines. */
char *strip_markdown(const char *text);

/* ================================================================
 *  Phone Number Redaction — for logging
 * ================================================================ */

/* Redact a phone number, preserving country code and last 4 digits.
 * Returns a malloc'd string. Input "<none>" returns "<none>". */
char *redact_phone(const char *phone);

/* ================================================================
 *  Provider Error Sanitization — map raw provider errors to user-safe replies
 *  Port of Python gateway/run.py _sanitize_gateway_final_response()
 * ================================================================ */

/* Check if text looks like a provider/infrastructure error (not normal content).
 * Mirrors Python _looks_like_gateway_provider_error().
 * Two heuristics: text is short (≤3 lines) AND error marker at start. */
bool gateway_looks_like_provider_error(const char *text);

/* Map a raw provider error to a short user-safe reply.
 * Mirrors Python _gateway_provider_error_reply().
 * Returns a malloc'd string (caller must free). */
char *gateway_provider_error_reply(const char *text);

/* Sanitize a gateway response before sending to chat.
 * For Telegram: redacts secrets, detects provider errors, rewrites them.
 * For other platforms: returns text as-is (secret redaction done elsewhere).
 * Returns a malloc'd string (caller must free), or NULL if text is NULL. */
char *gateway_sanitize_response(const char *platform, const char *text);

/* ================================================================
 *  Status Message Filtering — filter/sanitize agent status callbacks
 *  Port of Python gateway/run.py _prepare_gateway_status_message()
 * ================================================================ */

/* Filter a status message before platform delivery.
 * Returns NULL if message should be filtered out entirely.
 * Returns a malloc'd string (caller must free) otherwise. */
char *gateway_prepare_status_message(const char *platform, const char *text);

/* ================================================================
 *  Thread Participation Tracker — persistent JSON file tracking
 * ================================================================ */

typedef struct {
    char   **thread_ids;    /* array of thread ID strings */
    int     count;
    int     max_tracked;    /* default 500 */
    char    platform[64];   /* platform name, for state file path */
    char    state_dir[512]; /* hermes_home directory */
} thread_tracker_t;

/* Initialize a thread participation tracker.
 * platform: platform name (e.g. "discord", "matrix").
 * state_dir: hermes_home path for the JSON state file. */
void thread_tracker_init(thread_tracker_t *t, const char *platform,
                         const char *state_dir);

/* Load persisted threads from the JSON state file. */
void thread_tracker_load(thread_tracker_t *t);

/* Mark thread_id as participated and persist. */
void thread_tracker_mark(thread_tracker_t *t, const char *thread_id);

/* Check if thread_id has been participated in. */
bool thread_tracker_has(thread_tracker_t *t, const char *thread_id);

/* Free internal resources. Does NOT free the struct itself. */
void thread_tracker_destroy(thread_tracker_t *t);

/* ================================================================
 *  Gateway restart helpers
 *  Port of Python gateway/restart.py.
 * ================================================================ */

/* Parse a drain timeout value, falling back to DEFAULT_RESTART_DRAIN_TIMEOUT (30s). */
double parse_restart_drain_timeout(const char *raw);

/* ================================================================
 *  WhatsApp identity normalization
 *  Port of Python gateway/whatsapp_identity.py.
 * ================================================================ */

/* Strip WhatsApp JID/LID syntax down to its stable numeric identifier.
 * Caller must free the returned string. */
char *normalize_whatsapp_identifier(const char *value);

/* Resolve WhatsApp phone/LID aliases via bridge session mapping files.
 * Returns a malloc'd JSON array (caller must json_free). */
#include "hermes_json.h"
json_node_t *expand_whatsapp_aliases(const char *identifier);

/* Return a stable WhatsApp sender identity across phone-JID/LID variants.
 * Caller must free the returned string. */
char *canonical_whatsapp_identifier(const char *identifier);

/* ================================================================
 *  Session context helpers
 *  Port of Python gateway/session_context.py.
 * ================================================================ */

/* Set the active session ID in both env and thread-local storage. */
void set_current_session_id(const char *session_id);

/* Set all session context environment variables. */
void set_session_vars(const char *platform, const char *chat_id,
                       const char *chat_name, const char *thread_id,
                       const char *user_id, const char *user_name,
                       const char *session_key);

/* Clear all session context environment variables. */
void clear_session_vars(void);

/* Read a session context variable by its HERMES_SESSION_* name.
 * Returns malloc'd string (caller must free). */
char *get_session_env(const char *name, const char *default_value);

/* ================================================================
 *  Process memory monitor
 *  Port of Python gateway/memory_monitor.py.
 * ================================================================ */

/* Get current process RSS in MB by reading /proc/self/statm.
 * Returns RSS in MB, or 0 if unavailable. */
int get_rss_mb(void);

/* ================================================================
 *  Channel directory helpers
 *  Port of Python gateway/channel_directory.py.
 * ================================================================ */

/* Normalize a channel query: strip #, trim, lowercase.
 * Returns malloc'd string (caller must free). */
char *normalize_channel_query(const char *value);

/* Build a session entry ID: chat_id or chat_id:thread_id.
 * Returns malloc'd string (caller must free), or NULL if chat_id empty. */
char *session_entry_id(const char *chat_id, const char *thread_id);

/* ================================================================
 *  Runtime footer helpers
 *  Port of Python gateway/runtime_footer.py.
 * ================================================================ */

/* Collapse $HOME to ~ in a path. Returns malloc'd string. */
char *home_relative_cwd(const char *cwd);

/* Drop vendor/ prefix from model name. Returns malloc'd string. */
char *model_short(const char *model);

/* ================================================================
 *  Delivery helpers
 *  Port of Python gateway/delivery.py.
 * ================================================================ */

/* Check if a string looks like a Telegram private chat ID (positive integer). */
bool looks_like_telegram_private_chat_id(const char *chat_id);

/* Check if a string looks like an integer. */
bool looks_like_int(const char *value);

/* Check if a delivery result JSON indicates failure. */
bool send_result_failed(const char *result_json);

/* Check if content is a silence-narration token (no actual reply). */
bool is_silence_narration(const char *content);

/* ================================================================
 *  Display config helpers
 *  Port of Python gateway/display_config.py.
 * ================================================================ */

/* Normalize a display value (lowercase). Returns malloc'd string. */
char *normalise_display_value(const char *value);

/* ================================================================
 *  Auto-continue and timestamp helpers
 *  Port of Python gateway/run.py.
 * ================================================================ */

/* Build home-target env var: e.g. "telegram" -> "TELEGRAM_HOME_CHANNEL". */
char *resolve_home_target_env(const char *platform_name);

/* Build home-thread env var: e.g. "telegram" -> "TELEGRAM_HOME_CHANNEL_THREAD_ID". */
char *resolve_home_thread_env(const char *platform_name);

/* Read an env var as float, falling back to default on typos/empty. */
double read_float_env(const char *name, double default_val);

/* Return true when an interruption is fresh enough to auto-continue.
 * window_secs <= 0 disables the gate. */
bool is_fresh_gateway_interruption(double timestamp, double now, double window_secs);

#ifdef __cplusplus
}
#endif

#endif /* GATEWAY_HELPERS_H */
