/* aux_accounting.h — ambient session-accounting context for auxiliary LLM
 * calls, faithful C11 port of agent/aux_accounting.py (issue #23270).
 *
 * The agent loop publishes (state_usage_db, session_id) at turn entry; the
 * auxiliary client records usage at its response-validation chokepoint.
 * ContextVar isolation maps to C11 _Thread_local: per-thread context, with
 * the token save/restore contract mirrored explicitly.
 * Implemented in src/agent/port_aux_accounting.c.
 */

#ifndef SLERMES_AUX_ACCOUNTING_H
#define SLERMES_AUX_ACCOUNTING_H

#include <stdbool.h>
#include "state_usage_db.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Saved previous context (the ContextVar token). */
typedef struct {
    state_usage_db_t *db;
    const char *session_id;
    bool valid;
} aux_accounting_token_t;

/* Publish the active session's accounting handles. NULL db / empty
 * session_id clears the context (Python: publishes None). Returns the token
 * for reset. session_id must outlive the turn (not copied — same lifetime
 * contract as the Python tuple reference). */
aux_accounting_token_t aux_set_accounting_context(state_usage_db_t *db,
                                                  const char *session_id);

/* Restore the previous context (pair with set). */
void aux_reset_accounting_context(aux_accounting_token_t token);

/* Get the active turn's handles; returns false (and NULLs) outside a turn. */
bool aux_get_accounting_context(state_usage_db_t **db_out,
                                const char **session_id_out);

/* Record an auxiliary response's usage against the ambient session.
 * Strictly best-effort: returns false (never fails hard) when no context is
 * published, the task is main-loop-accounted (moa_reference/moa_aggregator),
 * or all token counts are zero. model falls back to "unknown" when NULL/"".
 * has_estimated_cost=false maps Python's None cost. */
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
                      double estimated_cost_usd);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_AUX_ACCOUNTING_H */
