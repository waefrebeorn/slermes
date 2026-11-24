/* noc_w19_runtimeses_1b971e.h - public C API for agent/runtime_session_compression.py port.
 * The owner's _apply RPC stays Python-side; every projection/decision the mixin makes on its
 * arguments is ported here. */
#ifndef NOC_W19_RUNTIMESES_1B971E_H
#define NOC_W19_RUNTIMESES_1B971E_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_runtimeses_1b971e_handoff_rows @ agent/runtime_session_compression.py:_handoff_rows
 * Project live compaction dicts onto the owner's closed handoff fields; non-dicts pass through. */
json_t *w19_runtimeses_1b971e_handoff_rows(const json_t *messages);

/* PoP: w19_runtimeses_1b971e_cas_rows @ agent/runtime_session_compression.py:archive_and_compact
 * The held_for_cas projection: the closed handoff fields plus the _db_persisted marker the CAS
 * compare reads; non-dicts are dropped. */
json_t *w19_runtimeses_1b971e_cas_rows(const json_t *held);

/* PoP: w19_runtimeses_1b971e_declared_scope_identity @ agent/runtime_session_compression.py:RuntimeSessionCompressionMixin.declared_scope_identity
 * tuple(...identity) of the caller's lineage reply, as an array. */
json_t *w19_runtimeses_1b971e_declared_scope_identity(const json_t *identity);

/* PoP: w19_runtimeses_1b971e_is_explicit_fork_child @ agent/runtime_session_compression.py:RuntimeSessionCompressionMixin.is_explicit_fork_child
 * identity[0]; null when the identity is empty. */
json_t *w19_runtimeses_1b971e_is_explicit_fork_child(const json_t *identity);

/* PoP: w19_runtimeses_1b971e_receipt_journal @ agent/runtime_session_compression.py:RuntimeSessionCompressionMixin._compression_receipt_journal
 * Re-scope a receipt candidate onto the assignment's session when the reply carries a
 * worker_assignment; {"error": "outbox_scope_mismatch"} when the parent is another scope. */
json_t *w19_runtimeses_1b971e_receipt_journal(const json_t *candidate, const json_t *result,
                                              const json_t *scope);

/* PoP: w19_runtimeses_1b971e_fallback_streak @ agent/runtime_session_compression.py:RuntimeSessionCompressionMixin.set_compression_fallback_streak
 * max(0, int(streak)); {"error": "ValueError"} when int() would raise. */
json_t *w19_runtimeses_1b971e_fallback_streak(const json_t *streak);

/* PoP: w19_runtimeses_1b971e_ineffective_count @ agent/runtime_session_compression.py:RuntimeSessionCompressionMixin.set_compression_ineffective_count
 * max(0, int(count)); {"error": "ValueError"} when int() would raise. */
json_t *w19_runtimeses_1b971e_ineffective_count(const json_t *count);

/* PoP: w19_runtimeses_1b971e_recovery_deadline @ agent/runtime_session_compression.py:RuntimeSessionCompressionMixin.set_compression_recovery_deadline
 * {"value": max(0.0, float(deadline or 0.0)) or None} - the recorded 'compression.recovery' payload. */
json_t *w19_runtimeses_1b971e_recovery_deadline(const json_t *deadline);

/* PoP: w19_runtimeses_1b971e_session_readable @ agent/runtime_session_compression.py:RuntimeSessionCompressionMixin.get_session
 * Own sessions always read; a foreign id needs membership in the lineage's readable_ids
 * ({"error": "permission_denied"} otherwise). */
json_t *w19_runtimeses_1b971e_session_readable(const char *scope_session_id, const char *requested,
                                               const json_t *readable_ids);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_RUNTIMESES_1B971E_H */
