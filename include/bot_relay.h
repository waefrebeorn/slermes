/*
 * bot_relay.h — C11 port of tools/bot_relay.py (envelope/turn-lock machinery).
 *
 * Cross-connection bot DM relay: remote-target resolution against roster.json,
 * outbox envelope queueing, reply persistence, and the per-profile
 * cross-process turn lock (flock-based).
 */
#ifndef BOT_RELAY_H
#define BOT_RELAY_H

#include <stddef.h>
#include "hermes_json.h"

/* Directory / file names under the relay root (tools/bot_relay.py literals). */
#define BR_RELAY_DIR_NAME            "bot_relay"
#define BR_ROSTER_FILE               "roster.json"
#define BR_OUTBOX_DIR                "outbox"
#define BR_CLAIMED_DIR               "claimed"
#define BR_REPLIES_DIR               "replies"
#define BR_LOCKS_DIR                 "locks"
#define BR_TURN_WAIT_SECONDS_FALLBACK 120.0

/* EnvelopeRefusedError equivalent: set when enqueue_envelope returns NULL. */
typedef struct bot_relay_error {
    char reason[64];    /* e.g. "runtime_offline" */
    char message[512];  /* human text (str(exc) in Python) */
} bot_relay_error_t;

/* relay_root @ tools/bot_relay.py:relay_root — "<root>/bot_relay" into buf. */
const char *bot_relay_relay_root(const char *root, char *buf, size_t n);

/* _ensure_dirs @ tools/bot_relay.py:_ensure_dirs — mkdir outbox/claimed/replies. */
const char *bot_relay_ensure_dirs(const char *root, char *buf, size_t n);

/* read_remote_roster @ tools/bot_relay.py:read_remote_roster — normalized
 * agent rows from roster.json; empty array on any failure (never raises). */
json_t *bot_relay_read_remote_roster(const char *root);

/* resolve_remote_target @ tools/bot_relay.py:resolve_remote_target.
 * Returns a NEW json value: the matched roster row (object copy), the string
 * "ambiguous" when a bare form matches agents on several connections, or
 * NULL for no match. */
json_t *resolve_remote_target(const char *raw_target, const json_t *roster);

/* remote_target_forms @ tools/bot_relay.py:remote_target_forms — NEW json
 * array of ambiguity-aware target strings ("handle" or "handle@conn"). */
json_t *remote_target_forms(const json_t *roster);

/* enqueue_envelope @ tools/bot_relay.py:enqueue_envelope — atomically queue
 * an outbox envelope; returns the envelope object, or NULL with err filled
 * when refused (EnvelopeRefusedError). */
json_t *enqueue_envelope(const char *root, const json_t *target,
                         const char *message, const char *sender_profile,
                         const char *sender_handle, bot_relay_error_t *err);

/* write_reply @ tools/bot_relay.py:write_reply — persist replies/<id>.json;
 * full path written to path_buf. Returns 0, or -1 for an invalid id
 * (Python ValueError). */
int write_reply(const char *root, const char *envelope_id, const char *reply,
                const char *error, const char *reason,
                char *path_buf, size_t n);

/* local_delivery_command @ tools/bot_relay.py:local_delivery_command — argv
 * (json array of strings) delivering a DM into profile's Bot Chat locally. */
json_t *local_delivery_command(const char *profile, const char *query_file);

/* turn_wait_seconds @ tools/bot_relay.py:turn_wait_seconds — config
 * bot_mode.turn_wait_seconds, lazily read; fallback 120.0. */
double turn_wait_seconds(void);

/* turn_lock_path @ tools/bot_relay.py:turn_lock_path — per-profile lockfile
 * path into buf; returns buf or NULL when it does not fit. */
const char *turn_lock_path(const char *root, const char *profile,
                           char *buf, size_t n);

/* acquire_turn_lock @ tools/bot_relay.py:acquire_turn_lock — non-blocking
 * flock probe + retry until timeout_seconds (< 0 → turn_wait_seconds()).
 * Returns the held fd (caller MUST release_turn_lock it), or -1 when the
 * budget is exhausted (TurnBusyError). Lock path written to path_buf. */
int acquire_turn_lock(const char *root, const char *profile,
                      double timeout_seconds, char *path_buf, size_t n);

/* Release a lock acquired by acquire_turn_lock (the generator's finally). */
void release_turn_lock(int fd);

/* tools/bot_failure_reasons.py ports (promoted from bot_relay's inline
 * classifier — shared by write_reply and external callers). */
void bot_failure_classify_agent_error(const char *text, char *out, size_t n);
int bot_failure_is_auto_retryable(const char *reason);
const char *bot_failure_retry_action(const char *reason);

#endif /* BOT_RELAY_H */
