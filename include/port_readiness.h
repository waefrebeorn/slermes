/*
 * port_readiness.h — pure classifiers from gateway/readiness.py.
 *
 * Each probe is a pure function of the runtime facts it reads (disk usage,
 * sqlite/config IO, runtime status) — those reads are the unported half, taken
 * here as parameters. Every function returns a malloc'd JSON dict string
 * matching the Python `_check(...)` output; caller frees.
 */
#ifndef PORT_READINESS_H
#define PORT_READINESS_H

/* _probe_state_db: exists=path present; corrupt=storage latch set;
 * error_name=the read-exception detail class name ("corrupt" or an exc name,
 * NULL means the read succeeded). */
char *rd_probe_state_db(int exists, int corrupt, const char *error_name);

/* _probe_config: config_exists, backend_unavailable, exc_name (NULL if the
 * read succeeded), raw_is_mapping (raw is None or dict). */
char *rd_probe_config(int config_exists, int backend_unavailable,
                      const char *exc_name, int raw_is_mapping);

/* _probe_disk: used_percent is the already-rounded value _probe_disk computes
 * (round(used/total*100,1)); error_name NULL when disk_usage succeeded. */
char *rd_probe_disk(double used_percent, long long free_bytes,
                    const char *error_name);

/* _probe_gateway: gateway_state (the runtime gateway_state string) and
 * platforms_json (a JSON object of platform -> (state|status)). */
char *rd_probe_gateway(const char *gateway_state, const char *platforms_json);

/* _probe_session_store: state_db_detail/state_db_status from the state_db
 * probe; store_state is the resolved session-store state ("", "ok",
 * "unavailable", "retrying", "unknown"). */
char *rd_probe_session_store(const char *state_db_detail,
                             const char *state_db_status,
                             const char *store_state);

/* collect_runtime_readiness overall status: "ok" when every check object in
 * statuses_json (a JSON array of {"status":...}) has status == "ok", else
 * "degraded". Returns a malloc'd "ok"/"degraded". */
char *rd_overall(const char *statuses_json);

#endif /* PORT_READINESS_H */
