/*
 * noc_w31_sessionaut_4e7286.h - C surface for upstream
 * tui_gateway/session_auto_continue.py (gate w31_sessionaut_4e7286).
 *
 * The pure value -> value core of the auto-continue module: the crash-recovery
 * note assembly, the ``desktop.auto_continue`` config coercion, the
 * busy-submit mode precedence, the server-queue fold (self-duplicate scrub,
 * lossless text merge, drain hand-off) and the two live-session projections.
 *
 * Every seam stays in the caller: the wall clock, the pid, the durable turn
 * marker, the loaded config.yaml root, the marker-writer probe, the config
 * file, the history lock, the DB and the worker threads. They arrive here as
 * fixture inputs, so each decision is a function of values alone.
 */
#ifndef NOC_W31_SESSIONAUT_4E7286_H
#define NOC_W31_SESSIONAUT_4E7286_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _AUTO_CONTINUE_FRESHNESS_MINUTES_DEFAULT */
#define W31_SESSIONAUT_4E7286_FRESHNESS_MINUTES_DEFAULT 15
/* _auto_continue_config's max_attempts default */
#define W31_SESSIONAUT_4E7286_MAX_ATTEMPTS_DEFAULT 2

/* prompt -> the self-describing recovery note (note prefix + embedded prompt). */
json_t *w31_sessionaut_4e7286_note(const json_t *prompt);

/* cfg.yaml root -> [enabled, freshness_window_seconds, max_attempts]. */
json_t *w31_sessionaut_4e7286_config(const json_t *cfg_root);

/* (client_queue_drain, configured display.busy_input_mode, compression_in_flight)
 * -> the effective mode ("queue" wins over a demoted steer/interrupt). */
json_t *w31_sessionaut_4e7286_busy_mode(const json_t *queued, const json_t *configured,
                                        const json_t *compression);

/* session -> the live turn's original user text, stripped ("" when absent). */
json_t *w31_sessionaut_4e7286_inflight_original(const json_t *session);

/* (queued envelope, inflight original) -> envelope | null (drop) | rewritten envelope. */
json_t *w31_sessionaut_4e7286_sanitize_entry(const json_t *entry, const json_t *original);

/* (session, entries) -> session with queued_prompt=head + queued_prompts=rest. */
json_t *w31_sessionaut_4e7286_set_queue(const json_t *session, const json_t *entries);

/* (session, text, transport, image_paths, turn_author) ->
 * {"envelope": <envelope|null>, "session": <post-drop/merge session>}. */
json_t *w31_sessionaut_4e7286_enqueue(const json_t *session, const json_t *text,
                                      const json_t *transport, const json_t *image_paths,
                                      const json_t *turn_author);

/* session -> the resumable inflight bubble (or null for no turn). */
json_t *w31_sessionaut_4e7286_inflight_snapshot(const json_t *session);

/* session -> {"user": <queued text>} or null. */
json_t *w31_sessionaut_4e7286_queued_snapshot(const json_t *session);

/* (session, marker, cfg_root, writer_state, now, pid) ->
 * {"out": <null|{"attempt","interrupted_at"}>, "cleared": <bool>,
 *  "scheduled": <bool>} -- the whole _maybe_schedule_auto_continue decision. */
json_t *w31_sessionaut_4e7286_maybe_schedule(const json_t *session, const json_t *marker,
                                             const json_t *cfg_root, const json_t *writer_state,
                                             const json_t *now, const json_t *pid);

#ifdef __cplusplus
}
#endif
#endif
