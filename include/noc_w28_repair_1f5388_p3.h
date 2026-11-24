/*
 * noc_w28_repair_1f5388_p3.h - C surface for upstream
 * persistence/sqlite/_state/repair.py, gate w28_repair_1f5388_p3.
 *
 * The report / preflight / strategy-ladder layer: the report shape
 * repair_state_db_schema returns, the _repair_skip mutation (including the
 * private attempt marker), the escalation ladder's outcome classification,
 * the entry-point branch plan (missing file, lost lock, live writer, exhausted
 * budget), the writability preflight's skip rule and its exact refusal text,
 * and the strategy table with its pinned log messages.
 *
 * Seams (not here): the sqlite connection, os.access/chmod, the filesystem,
 * the hermes home, logging, the startup watchdog.
 */
#ifndef NOC_W28_REPAIR_1F5388_P3_H
#define NOC_W28_REPAIR_1F5388_P3_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* repair_state_db_schema()'s initial report. */
json_t *w28_repair_1f5388_initial_report(void);

/* _repair_skip(report, verb, error, exc): sets the private
 * "_repair_attempted" marker when the exception proves deterministic
 * corruption, then "error"; returns the same report. */
json_t *w28_repair_1f5388_repair_skip(const json_t *report, int has_exc, int is_db_error,
                                      int errorcode_present, double errorcode,
                                      const char *message, const char *error);

/* _run_repair_strategies' ladder.  `reasons` is one entry per strategy in
 * _REPAIR_STRATEGIES order: null = opens cleanly, a string = that probe
 * reason, {"unavailable": text} = HealthCheckUnavailable.  Returns the report
 * keys the ladder decides (repaired/strategy/error/_repair_attempted). */
json_t *w28_repair_1f5388_run_strategies(const json_t *reasons);

/* repair_state_db_schema() before any surgery: scenario is
 * "missing" | "lock_lost" | "lock_lost_healed" | "live_writer" | "exhausted".
 * `profile_arg` is hermes_constants.profile_cli_selector()'s value. */
json_t *w28_repair_1f5388_repair_flow(const char *scenario, const char *db_path,
                                      const char *profile_arg);

/* preflight_db_writability's early return: ":memory:" and "file:" URIs. */
json_t *w28_repair_1f5388_preflight_skip(const char *db_path);

/* _rw_or_vanished(): rw_or_vanished(p) = access(p, R_OK|W_OK) or not
 * p.exists() or access(p, R_OK|W_OK) again. */
json_t *w28_repair_1f5388_rw_or_vanished(int rw_first, int exists, int rw_second);

/* preflight_db_writability's refusal text (sqlite3.OperationalError). */
json_t *w28_repair_1f5388_preflight_error(const char *db_label, const char *path, int is_dir);

/* _unlink_db_triple(): every path it unlink()s, in order. */
json_t *w28_repair_1f5388_unlink_victims(const char *db_path);

/* _REPAIR_STRATEGIES: [{"name","success","failure"}, ...] in escalation order
 * (failure null on the final strategy). */
json_t *w28_repair_1f5388_strategies(void);

#ifdef __cplusplus
}
#endif
#endif
