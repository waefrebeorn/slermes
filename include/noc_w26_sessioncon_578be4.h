/*
 * noc_w26_sessioncon_578be4.h - C surface for upstream
 * gateway/session_control_actions.py (gate w26_sessioncon_578be4).
 *
 * The card-button core of ``session.control``: which typed slash command each
 * Desktop goal/loop/heartbeat button stands for, the admission rules checked
 * before anything runs, the envelope a result is trimmed to, and the ordering
 * guard that keeps a stale control snapshot from repainting newer state.
 * Every seam (authority rows, the goal manager, execute_command, the event
 * stream) stays in the caller; this file owns only what the values decide.
 */
#ifndef NOC_W26_SESSIONCON_578BE4_H
#define NOC_W26_SESSIONCON_578BE4_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req: {action, args} -> "ok" when _validate accepts it, else the refusal
 * reason ("invalid_params"). */
json_t *w26_sessioncon_578be4_validate(const json_t *req);

/* action -> ["subgoal"] when it is a criterion button, [name, arg] when it is
 * a typed slash command, null when it is neither. */
json_t *w26_sessioncon_578be4_dispatch(const json_t *action);

/* result -> the fixed envelope object (type, output, notice, message, display),
 * missing members as null. */
json_t *w26_sessioncon_578be4_envelope(const json_t *result);

/* req: {control_published_seq, control_revision, ticket, snapshot_revision,
 *       only_if_present, only_if_changed} ->
 * {published: bool, control_published_seq, control_revision}.  The event stream
 * is the seam: this is the state _publish_snapshot leaves behind. */
json_t *w26_sessioncon_578be4_publish_decision(const json_t *req);

/* The button -> command table, the criterion set and the envelope keys. */
json_t *w26_sessioncon_578be4_constants(void);

#ifdef __cplusplus
}
#endif
#endif
