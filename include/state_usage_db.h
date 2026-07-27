/* state_usage_db.h — session_model_usage plumbing, faithful C11 port of the
 * usage-accounting surface of hermes_state.py (SessionDB._insert_session_row
 * guard, _record_model_usage upsert, record_auxiliary_usage).
 *
 * Operates on the sqlite state DB (state.db — same store src/app_state.c
 * opens). Implemented in src/cli/port_state_usage_db.c over the vendored
 * sqlite3 in lib/libdb. Opaque handle; no god headers.
 */

#ifndef SLERMES_STATE_USAGE_DB_H
#define SLERMES_STATE_USAGE_DB_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct state_usage_db state_usage_db_t;

/* Open (and ensure schema for) the usage tables in the sqlite DB at *path*.
 * Creates sessions/session_model_usage tables when missing (mirror of the
 * Python schema DDL). Returns NULL on failure. */
state_usage_db_t *state_usage_db_open(const char *path);
void state_usage_db_close(state_usage_db_t *db);

/* INSERT OR IGNORE a bare session row (FK guard — mirror of
 * _insert_session_row's role in record_auxiliary_usage). */
bool state_usage_insert_session_row(state_usage_db_t *db,
                                    const char *session_id,
                                    const char *source);

/* Per-(model,provider,base_url,mode,task) usage delta upsert — faithful port
 * of SessionDB._record_model_usage. NULL model/provider/base_url/mode follow
 * the Python fallback rules: aux rows (task != "") never inherit the session
 * route; main-loop rows (task == "") COALESCE from the session row. Pass
 * has_estimated_cost=false for Python None (adds 0.0, keeps column sum). */
bool state_usage_record_model_usage(state_usage_db_t *db,
                                    const char *session_id,
                                    const char *model,
                                    const char *billing_provider,
                                    const char *billing_base_url,
                                    const char *billing_mode,
                                    const char *task,
                                    int api_call_count,
                                    long long input_tokens,
                                    long long output_tokens,
                                    long long cache_read_tokens,
                                    long long cache_write_tokens,
                                    long long reasoning_tokens,
                                    bool has_estimated_cost,
                                    double estimated_cost_usd,
                                    bool has_actual_cost,
                                    double actual_cost_usd,
                                    const char *cost_status,
                                    const char *cost_source);

/* Faithful port of SessionDB.record_auxiliary_usage: no-op on empty
 * session_id/task, FK guard insert, then the aux-task delta (api_call_count=1,
 * billing_mode NULL, no actual cost/status/source). */
bool state_usage_record_auxiliary_usage(state_usage_db_t *db,
                                        const char *session_id,
                                        const char *task,
                                        const char *model,
                                        const char *billing_provider,
                                        const char *billing_base_url,
                                        long long input_tokens,
                                        long long output_tokens,
                                        long long cache_read_tokens,
                                        long long cache_write_tokens,
                                        long long reasoning_tokens,
                                        bool has_estimated_cost,
                                        double estimated_cost_usd);

/* Read back an accumulated row (test/verification surface). Returns false
 * when the row does not exist. Any out pointer may be NULL. */
bool state_usage_get_row(state_usage_db_t *db,
                         const char *session_id,
                         const char *model,
                         const char *task,
                         int *api_call_count,
                         long long *input_tokens,
                         long long *output_tokens,
                         long long *reasoning_tokens,
                         double *estimated_cost_usd);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_STATE_USAGE_DB_H */
