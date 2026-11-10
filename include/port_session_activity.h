#ifndef PORT_SESSION_ACTIVITY_H
#define PORT_SESSION_ACTIVITY_H

#include "hermes_json.h"

/* Port of Python: agent/session_activity.py — the pure activity-stamp core.
 *
 * Provenance constants mirror the ActivityProvenance enum + the terminal
 * compression set. `reset_session_activity_persist_window` (agent state
 * write) and the SessionDB heartbeat writer are runtime and NOT ported.
 * `format_iteration_progress` renders the canonical English templates from
 * locales/en.yaml ("iteration {n}" / "iteration {n}/{max}") — the untranslated
 * default the product ships with.
 */

#define SA_ACTIVITY_DESCRIPTION_MAX 120
#define SA_SESSION_ACTIVITY_HEARTBEAT_MIN_INTERVAL_SECONDS 60.0

/* ActivityProvenance values. */
#define SA_PROV_UNKNOWN "unknown"
#define SA_PROV_AGENT_COMPRESSION "agent.compression"
#define SA_PROV_AGENT_COMPRESSION_TIMEOUT "agent.compression_timeout"
#define SA_PROV_AGENT_COMPRESSION_COOLDOWN "agent.compression_cooldown"
#define SA_PROV_AGENT_COMPRESSION_TURNHOLD "agent.compression_turnhold"

/* True when the provenance is one of the terminal compression stamps (must
 * bypass the persist rate limit). Unknown/empty -> false. */
int sa_is_terminal_compression_provenance(const char *provenance);

/* One of the known provenance values, or SA_PROV_UNKNOWN when unset. Returns
 * a malloc'd string the caller must free. */
char *sa_normalize_activity_provenance(const char *provenance);

/* strip() then clamp to 120 chars, ellipsizing the 121+ case with U+2026.
 * Returns a malloc'd string. */
char *sa_bound_activity_description(const char *description);

/* "iteration N" (unbounded cap) for cap >= sys.maxsize, else "iteration N/M".
 * n_text is the caller's rendered count. malloc'd result. */
char *sa_format_iteration_progress(const char *n_text, const char *max_iterations);

/* The shared activity snapshot dict (at / description / provenance /
 * seconds_since_activity rounded to 0.1, plus the short alias keys and extra).
 * last_activity_at may be NULL. Returns a JSON object. */
json_t *sa_build_activity_snapshot(const char *last_activity_at,
                                   const char *last_activity_description,
                                   const char *last_activity_provenance,
                                   double now, const char *extra_json,
                                   int has_extra);

/* AwakeIdleMeter::measure — idle seconds minus host sleep. wall/mono are the
 * caller's current clocks (Python: time.time() / time.monotonic()). */
typedef struct {
    double wall, mono, asleep_s;
} sa_idle_meter_t;

void sa_idle_meter_init(sa_idle_meter_t *m, double wall, double mono);
double sa_measure_idle(sa_idle_meter_t *m, double idle_s, double wall, double mono);

#endif /* PORT_SESSION_ACTIVITY_H */
