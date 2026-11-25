/*
 * noc_w34_reconcile_7686f7_p1.h - C surface for upstream
 * persistence/sqlite/admin/_reconcile.py (gate w34_reconcile_7686f7_p1).
 *
 * Store identity, the advisory text and the remedy commands of the additive
 * schema-drift lane.  Every filesystem read, sqlite handle and clock is a
 * caller-supplied value: this surface owns only what those values mean --
 * which profile a home directory belongs to, what a gateway.lock record says
 * about who owns the store, and which command the user is told to run.
 */
#ifndef NOC_W34_RECONCILE_7686F7_P1_H
#define NOC_W34_RECONCILE_7686F7_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* raw = the text a status file holds (utf-8-sig, as read) ->
 * the JSON object it carries, {"pid": N} for a bare-integer PID file when
 * bare_pid_ok, else null.  Absent/empty/unreadable/invalid all yield null. */
json_t *w34_reconcile_7686f7_p1_json_file(const char *raw, int bare_pid_ok);

/* Host-platform path identity: true when both spellings denote one home
 * (~ expansion and symlink resolution are the caller's seam). */
json_t *w34_reconcile_7686f7_p1_same_home(const char *left, const char *right);

/* "<root>/profiles/<name>" -> "name" when <name> is a valid profile label,
 * else null (the platform-default-home probe is a seam: the caller resolves
 * "default" itself). */
json_t *w34_reconcile_7686f7_p1_profile_label(const char *path);

/* The `hermes sessions repair` invocation that targets *home*. */
json_t *w34_reconcile_7686f7_p1_repair_command(const char *home);

/* [label-or-home, restart command] for the gateway that holds *home*. */
json_t *w34_reconcile_7686f7_p1_owner_remedy(const char *home);

/* Restart command for the gateway holding *home*, from the raw gateway.lock
 * text (null when the lock is absent): a multiplexer launched from another
 * home names that home in `hermes_home`, and its own command is the remedy. */
json_t *w34_reconcile_7686f7_p1_owning_gateway_restart(const char *home,
                                                       const char *lock_text);

/* The malformed-image message a lagging-and-damaged store is refused with. */
json_t *w34_reconcile_7686f7_p1_lagging_damaged(const char *detail);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W34_RECONCILE_7686F7_P1_H */
