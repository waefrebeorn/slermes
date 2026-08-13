#ifndef AGENT_ESTOP_H
#define AGENT_ESTOP_H

/* Port of agent/estop.py — global emergency stop (resumable pause for
 * new work only). Sentinel file under HERMES_HOME/ESTOP.
 *
 * Faithful to the Python: fail-SAFE on stat errors (engaged), a corrupt/empty
 * sentinel body still counts as engaged, the pause is authoritative while
 * metadata (reason/engaged_at) is best-effort. */

/* Path of the ESTOP sentinel under the active HERMES_HOME (caller frees). */
char *estop_sentinel_path(void);

/* One stat: is the global emergency stop engaged? Fail-SAFE. */
int estop_is_engaged(void);

/* Create the ESTOP sentinel. Idempotent; re-engage updates the file body.
 * Returns the sentinel path (caller frees); NULL on failure to write. */
char *estop_engage(const char *reason);

/* Remove the sentinel. Returns 1 if a pause was lifted, 0 if none. */
int estop_disengage(void);

/* Return {"reason": ..., "engaged_at": ...} as a malloc'd JSON string, or
 * NULL when not engaged. A corrupt body still reports engaged with null
 * fields. Caller frees. */
char *estop_get_state(void);

/* Short user-facing notice for new gateway turns, or NULL if not paused
 * (caller frees). */
char *estop_paused_reply(void);

/* Return 1 when engaged, logging once per engagement per component.
 * `component` is the caller name; uses stderr logging (mirror of logger.info). */
int estop_check_paused(const char *component);

/* Test isolation helper: clear the log-once bookkeeping. */
void estop_reset_log_state_for_tests(void);

#endif /* AGENT_ESTOP_H */
