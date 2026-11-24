#ifndef NOC_W20_DISPATCH_C9FBE4_H
#define NOC_W20_DISPATCH_C9FBE4_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of tools/connectors/dispatch.py (gate w20_dispatch_c9fbe4).
 *
 * Connector calls routed through the normal dispatch policy pipeline.  The
 * gateway bridge, the local tool executor and the interrupt flag are injected
 * as scripted seams; partitioning, result splicing, per-entry call ids and the
 * batch/too-many/interrupted decisions are the real C.
 */

/* PoP: w20_dispatch_c9fbe4_partition @ tools/connectors/gateway/merge.py:partition_calls
 * Splits a batch into local, remote and malformed-name entries.
 * Request: {"calls": [{"name":..., "arguments": {...}}, ...]}.
 * Answer: {"local": [[position, call], ...],
 *          "remote": [{"position", "name", "connector", "tool", "arguments"}, ...],
 *          "errors": [<error entry>, ...]}. */
json_t *w20_dispatch_c9fbe4_partition(const json_t *req);

/* PoP: w20_dispatch_c9fbe4_connector_call @ tools/connectors/dispatch.py:dispatch_connector_call
 * Request: {"name", "arguments", "tool_call_id", "turn_id", "task_id",
 *           "entries": [<gateway entry>, ...]}   (the scripted run_remote answer)
 * Answer: {"result": {"response"|"error"|"account": ...}, "data_use": [...]|null}
 * -- exactly the three keys the model-facing payload keeps, plus the data-use
 * names this call recorded.  An empty entry list raises IndexError. */
json_t *w20_dispatch_c9fbe4_connector_call(const json_t *req);

/* PoP: w20_dispatch_c9fbe4_connector_batch @ tools/connectors/dispatch.py:dispatch_connector_batch
 * Request:
 *   {"calls": [{"name","arguments"}, ...],
 *    "ids": {"tool_call_id": str|null, ...},
 *    "payloads": [<str|json>, ...],        the scripted handle_function_call answers,
 *                                          "$ID$" expands to the call id it was handed
 *    "interrupted_at": int|null,           index at which is_interrupted() turns true
 *    "local_error": str}                   the local-batch rejection text
 * Answer: the parsed result object of the returned JSON text:
 *   {"results": [...], "success_count": N, "error_count": M, "total_count": T}
 * or {"error": "<message>"} for a whole-batch refusal. */
json_t *w20_dispatch_c9fbe4_connector_batch(const json_t *req);

/* PoP: w20_dispatch_c9fbe4_max_calls @ tools/connectors/gateway/config.py:MAX_CALLS_PER_DISPATCH
 * The context cap one dispatch accepts (10). */
int w20_dispatch_c9fbe4_max_calls(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_DISPATCH_C9FBE4_H */
