/*
 * noc_w26_processreg_e923be.h - C surface for upstream
 * tools/process_registry_awaiting.py (gate w26_processreg_e923be).
 *
 * Whether a background process will still report back to the agent that
 * spawned it.  The live module answers that from live registry state under two
 * locks; this surface takes that state as values (queued events, alive running
 * entries, finished entries, the in-flight counter) and makes exactly the same
 * decision.  Nothing here takes a lock, starts a thread or looks at a pid.
 */
#ifndef NOC_W26_PROCESSREG_E923BE_H
#define NOC_W26_PROCESSREG_E923BE_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* SUPERVISOR-ready helpers: the raw spawning owner of a completion event. */
json_t *w26_processreg_e923be_owner(const json_t *evt);

/* _notifies: does this session's completion still produce a notice? */
json_t *w26_processreg_e923be_notifies(const json_t *session);

/* retained_for_delivery: finished, claimed, completion not yet published. */
json_t *w26_processreg_e923be_retained(const json_t *session);

/* _adjust: in-flight hold counter after owners gain/lose one event each.
 * A key whose count drops to <= 0 is deleted, like Counter/_adjust does. */
json_t *w26_processreg_e923be_adjust(const json_t *in_flight,
                                     const json_t *owners,
                                     const json_t *delta);

/* awaits_notification over a fixed registry snapshot:
 * {"owners": [...], "running": [session], "finished": [session],
 *  "queued": [event], "in_flight": {"owner": n}} */
json_t *w26_processreg_e923be_awaits(const json_t *state);

#ifdef __cplusplus
}
#endif
#endif
