/*
 * port_gateway_run_deps.h — Faithful C11 ports of gateway/run.py helpers that
 * are thin wrappers over already-ported dependency subsystems (goals, gateway
 * status, profiles). These closed once their dependency subsystems existed in
 * C; they reuse those subsystems via their opaque public APIs.
 */

#ifndef PORT_GATEWAY_RUN_DEPS_H
#define PORT_GATEWAY_RUN_DEPS_H

#include <stdbool.h>

/* run.py _active_profile_name — profile name this gateway represents.
 * Wraps profiles.get_active_profile_name(); "default" on any failure.
 * Returns a malloc'd string (caller frees). */
char *gw_active_profile_name(void);

/* run.py _goal_still_active_for_session — best-effort fresh DB check before
 * running a queued continuation. Reads the persisted goal state for
 * session_id from state.db (state_meta key "goal:<session_id>") and returns
 * true only when its status is "active". False on empty session_id or any
 * failure. */
bool gw_goal_still_active_for_session(const char *session_id);

/* run.py _update_platform_runtime_status — write a per-platform runtime status
 * record. Wraps gateway.status.write_runtime_status(); best-effort (errors
 * swallowed, matching the Python try/except pass). NULL args leave the
 * corresponding field unchanged. */
void gw_update_platform_runtime_status(const char *platform,
                                       const char *platform_state,
                                       const char *error_code,
                                       const char *error_message);

#endif /* PORT_GATEWAY_RUN_DEPS_H */
