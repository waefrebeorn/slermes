/*
 * noc_w23_basesessio_9eb9cc.h - C surface for upstream
 * gateway/platforms/base_session_command.py (gate w23_basesessio_9eb9cc).
 *
 * "/stop", "/new" and "/reset" reaching a busy session: the command takes a
 * guard of its own, replies inline, and only cancels the session on the
 * runner's verdict.  A refusal hands the session back exactly as the command
 * found it -- the owner's guard to a live owner, a flush (or a drain) to a
 * newer owner, and the held text to whoever drains it.
 *
 * Seams (injected by the harness, never decided here): the adapter's active
 * session table, its session tasks, the authorization check, the event's
 * interrupt verdict, the text-debounce store and the guard ledger
 * (_replaced_guard / _in_flight).  Guards are named by the fixture; identity is
 * the name.
 */
#ifndef NOC_W23_BASESESSIO_9EB9CC_H
#define NOC_W23_BASESESSIO_9EB9CC_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The guard the fixture names for the command's own, freshly created guard. */
#define W23_BASESESSIO_9EB9CC_COMMAND_GUARD "<command>"

/* world keys: guard, replaced[[child,parent|null]].
 * Returns the guard under every command guard, or null. */
json_t *w23_basesessio_9eb9cc_guard_under(const json_t *world);

/* world keys: guard, in_flight[guard]. */
json_t *w23_basesessio_9eb9cc_in_flight(const json_t *world);

/* world keys: current_guard, replaced, in_flight, held, auth_check,
 * interrupt_accepted, newer_owner, in_session_tasks, task_stale.
 * Returns {"calls":[...],"active":name|null,"held_restored":bool,
 * "in_flight_after":count}. */
json_t *w23_basesessio_9eb9cc_dispatch(const json_t *world);

#ifdef __cplusplus
}
#endif
#endif
