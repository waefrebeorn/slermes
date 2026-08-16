/* port_aux_accounting.c — faithful C11 port of agent/aux_accounting.py.
 *
 * ContextVar -> C11 _Thread_local. The Python module's isolation properties
 * map as: concurrent agents (threads) never see each other's context;
 * a worker thread inherits nothing unless the spawner republishes (the C
 * thread-context propagation helper does exactly that, mirroring
 * tools.thread_context.propagate_context_to_thread).
 *
 * MoA reference/aggregator slots are EXCLUDED: conversation_loop already
 * folds their usage into the main update_token_counts delta — recording
 * them here would double-count (_EXCLUDED_TASKS).
 */

#include "aux_accounting.h"
#include <string.h>

/* (session_db, session_id) for the active agent turn, or cleared. */
static _Thread_local state_usage_db_t *g_acct_db = NULL;
static _Thread_local const char *g_acct_session_id = NULL;

/* PoP: aux_set_accounting_context @ agent/aux_accounting.py:set_accounting_context */
aux_accounting_token_t aux_set_accounting_context(state_usage_db_t *db,
                                                  const char *session_id) {
    aux_accounting_token_t tok = {
        .db = g_acct_db, .session_id = g_acct_session_id, .valid = true,
    };
    if (db == NULL || session_id == NULL || !*session_id) {
        g_acct_db = NULL;          /* publishing None clears the context */
        g_acct_session_id = NULL;
    } else {
        g_acct_db = db;
        g_acct_session_id = session_id;
    }
    return tok;
}

/* PoP: aux_reset_accounting_context @ agent/aux_accounting.py:reset_accounting_context */
void aux_reset_accounting_context(aux_accounting_token_t token) {
    if (token.valid) {
        g_acct_db = token.db;
        g_acct_session_id = token.session_id;
    } else {
        g_acct_db = NULL;          /* Python except-branch: set(None) */
        g_acct_session_id = NULL;
    }
}

/* PoP: aux_get_accounting_context @ agent/aux_accounting.py:get_accounting_context */
bool aux_get_accounting_context(state_usage_db_t **db_out,
                                const char **session_id_out) {
    if (db_out) *db_out = g_acct_db;
    if (session_id_out) *session_id_out = g_acct_session_id;
    return g_acct_db != NULL && g_acct_session_id != NULL;
}

/* PoP: aux_record_usage @ agent/aux_accounting.py:record_aux_usage */
bool aux_record_usage(const char *task,
                      const char *model,
                      const char *provider,
                      const char *base_url,
                      long long input_tokens,
                      long long output_tokens,
                      long long cache_read_tokens,
                      long long cache_write_tokens,
                      long long reasoning_tokens,
                      bool has_estimated_cost,
                      double estimated_cost_usd) {
    /* if not task or task in _EXCLUDED_TASKS: return */
    if (!task || !*task) return false;
    if (strcmp(task, "moa_reference") == 0 ||
        strcmp(task, "moa_aggregator") == 0)
        return false;

    state_usage_db_t *db = NULL;
    const char *session_id = NULL;
    if (!aux_get_accounting_context(&db, &session_id)) return false;

    /* if not (any token bucket non-zero): return */
    if (!(input_tokens || output_tokens || cache_read_tokens ||
          cache_write_tokens || reasoning_tokens))
        return false;

    const char *eff_model = (model && *model) ? model : "unknown";
    /* Best-effort by contract: any store failure is swallowed here (the
     * store itself reports, but an aux call must never fail on it). */
    return state_usage_record_auxiliary_usage(
        db, session_id, task, eff_model, provider, base_url,
        input_tokens, output_tokens, cache_read_tokens, cache_write_tokens,
        reasoning_tokens, has_estimated_cost, estimated_cost_usd);
}
