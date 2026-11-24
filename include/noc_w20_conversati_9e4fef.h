#ifndef NOC_W20_CONVERSATI_9E4FEF_H
#define NOC_W20_CONVERSATI_9E4FEF_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of agent/conversation_compression_codex.py (gate w20_conversati_9e4fef).
 *
 * The module routes compaction to the Codex app-server for Codex-owned
 * threads.  Everything it touches (heartbeat, status/warning emitters, the
 * telemetry sink, codex_runtime bookkeeping) is an agent capability, so the
 * port models the decision, the log lines and every observable effect of one
 * call, with those capabilities supplied as data. */

/* PoP: w20_conversati_9e4fef_auto_mode @ agent/conversation_compression_codex.py:_compress_context_via_codex_app_server
 * str(raw or "native").lower(), clamped to {native, hermes, off}.  Caller frees. */
char *w20_conversati_9e4fef_auto_mode(const char *raw);

/* PoP: w20_conversati_9e4fef_tokens_label @ agent/conversation_compression_codex.py:_compress_context_via_codex_app_server
 * f"{approx_tokens:,}" when truthy, else "unknown".  Caller frees. */
char *w20_conversati_9e4fef_tokens_label(const json_t *approx_tokens);

/* PoP: w20_conversati_9e4fef_cooldown_remaining @ agent/conversation_compression_codex.py:_codex_compaction_cooldown_remaining
 * agent = {"compressor": null | {"has_getter": bool, "raises": bool,
 *                                "state": null | {"remaining_seconds": v}}}
 * -> max(0.0, float(state["remaining_seconds"] or 0.0)) or 0.0. */
json_t *w20_conversati_9e4fef_cooldown_remaining(const json_t *agent);

/* PoP: w20_conversati_9e4fef_skip_decision @ agent/conversation_compression_codex.py:_compress_context_via_codex_app_server
 * in = {"force": bool, "auto_mode": <normalised>, "cooldown": number, "has_session": bool}
 * -> null, or {"reason", "status", "class"} exactly as the skipper computes them. */
json_t *w20_conversati_9e4fef_skip_decision(const json_t *in);

/* PoP: w20_conversati_9e4fef_failed @ agent/conversation_compression_codex.py:_compress_context_via_codex_app_server
 * bool(getattr(result,"interrupted",False) or getattr(result,"error",None)). */
int w20_conversati_9e4fef_failed(const json_t *result);

/* PoP: w20_conversati_9e4fef_failure_message @ agent/conversation_compression_codex.py:_compress_context_via_codex_app_server
 * The exact user-facing warning: "⚠ Codex app-server compaction failed: {result.error}". */
char *w20_conversati_9e4fef_failure_message(const json_t *result);

/* PoP: w20_conversati_9e4fef_failure_error @ agent/conversation_compression_codex.py:_compress_context_via_codex_app_server
 * str(getattr(result,"error",None) or "compaction interrupted") -- the string the
 * cooldown is armed with. */
char *w20_conversati_9e4fef_failure_error(const json_t *result);

/* PoP: w20_conversati_9e4fef_status_text @ agent/conversation_compression_codex.py:_compress_context_via_codex_app_server
 * The heartbeat stop label. */
const char *w20_conversati_9e4fef_status_text(int failed);

/* PoP: w20_conversati_9e4fef_plan @ agent/conversation_compression_codex.py:_compress_context_via_codex_app_server
 * One whole call, with the agent's capabilities supplied as data.  Input:
 *   force, auto_mode (raw attribute value), session_id, approx_tokens,
 *   messages (array; only its length matters), cooldown (seconds remaining),
 *   has_session, task_id, result (the compact_thread result object, or null
 *   to raise), raise_type, has_update_from_response, cooldown_recorder.
 * Output: auto_mode, session_id, tokens, logs[], record{commit_status,
 *   failure_class, route, method}, heartbeat_stop, warning, cooldown_record,
 *   retired, closed, bookkeeping, usage, dedup_reset, exception. */
json_t *w20_conversati_9e4fef_plan(const json_t *in);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_CONVERSATI_9E4FEF_H */
