/* hermes_state_tokens.c — token-counter + aux-usage methods of the SessionDB
 * port. update_token_counts (absolute/delta) is a faithful SQL mirror of
 * hermes_state.py; record_auxiliary_usage delegates to the shared
 * state_usage_db implementation (same sqlite handle layout — no duplication:
 * hermes_state_db_t and state_usage_db_t are both { sqlite3* }). Self-contained.
 */

#include "hermes_state_internal.h"
#include "hermes_state_db.h"
#include "state_usage_db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: update_token_counts @ hermes_state.py:update_token_counts */
bool hermes_state_update_token_counts(hermes_state_db_t *db,
                                      const char *session_id,
                                      long long input_tokens,
                                      long long output_tokens,
                                      long long cache_read_tokens,
                                      long long cache_write_tokens,
                                      long long reasoning_tokens,
                                      long long api_call_count,
                                      bool absolute) {
    if (!db || !session_id || !*session_id) return false;
    sqlite3_stmt *ins = NULL;
    if (sqlite3_prepare_v2(db->db,
            "INSERT OR IGNORE INTO sessions (id, source, started_at) "
            "VALUES (?, 'unknown', ?)", -1, &ins, NULL) == SQLITE_OK) {
        sqlite3_bind_text(ins, 1, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(ins, 2, hermes_state_now_epoch());
        sqlite3_step(ins);
        sqlite3_finalize(ins);
    }
    const char *sql = absolute
        ? "UPDATE sessions SET input_tokens=?, output_tokens=?, "
          "cache_read_tokens=?, cache_write_tokens=?, reasoning_tokens=?, "
          "api_call_count=? WHERE id=?"
        : "UPDATE sessions SET input_tokens=input_tokens+?, "
          "output_tokens=output_tokens+?, cache_read_tokens=cache_read_tokens+?, "
          "cache_write_tokens=cache_write_tokens+?, reasoning_tokens=reasoning_tokens+?, "
          "api_call_count=COALESCE(api_call_count,0)+? WHERE id=?";
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db, sql, -1, &st, NULL) != SQLITE_OK) return false;
    sqlite3_bind_int64(st, 1, input_tokens > 0 ? input_tokens : 0);
    sqlite3_bind_int64(st, 2, output_tokens > 0 ? output_tokens : 0);
    sqlite3_bind_int64(st, 3, cache_read_tokens > 0 ? cache_read_tokens : 0);
    sqlite3_bind_int64(st, 4, cache_write_tokens > 0 ? cache_write_tokens : 0);
    sqlite3_bind_int64(st, 5, reasoning_tokens > 0 ? reasoning_tokens : 0);
    sqlite3_bind_int64(st, 6, api_call_count > 0 ? api_call_count : 0);
    sqlite3_bind_text(st, 7, session_id, -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}

/* PoP: record_auxiliary_usage @ hermes_state.py:record_auxiliary_usage */
bool hermes_state_record_auxiliary_usage(hermes_state_db_t *db,
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
                                         double estimated_cost_usd) {
    if (!db || !session_id || !*session_id || !task || !*task) return false;
    state_usage_db_t *u = (state_usage_db_t *)db;
    return state_usage_record_auxiliary_usage(
        u, session_id, task, model, billing_provider, billing_base_url,
        input_tokens, output_tokens, cache_read_tokens, cache_write_tokens,
        reasoning_tokens, has_estimated_cost, estimated_cost_usd);
}
