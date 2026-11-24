/*
 * noc_w29_adapter_0e0bda_p3.h - C surface for upstream
 * plugins/platforms/a2a/adapter.py, gate p3 (JSON-RPC dispatch framing).
 *
 * The JSON-RPC handlers themselves touch the task store, the tool registry and
 * the HTTP writer; those are seams here.  What this gate owns is the pure half
 * of the dispatch path: the method table (v1.0 PascalCase names plus their
 * legacy slash aliases), the not-found / invalid-params error envelopes, the
 * inline push-config extraction, the forwarded-admission retry LRU and the
 * chat-info shape.
 */
#ifndef NOC_W29_ADAPTER_0E0BDA_P3_H
#define NOC_W29_ADAPTER_0E0BDA_P3_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* method -> {handler, isV1} for a known JSON-RPC method, else null. */
json_t *w29_adapter_0e0bda_method_lookup(const json_t *method);

/* The task-not-found error envelope (taskId or id, else ""). */
json_t *w29_adapter_0e0bda_find_task_error(const json_t *req_id, const json_t *params);

/* error envelope of the shared get/list/delete op: "taskId required", else
 * "push config not found for task: <id>". */
json_t *w29_adapter_0e0bda_push_op_error(const json_t *req_id, const json_t *params);

/* tasks/pushNotificationConfig/create: invalid-params envelope, else the
 * task-not-found envelope for a task the store does not hold. */
json_t *w29_adapter_0e0bda_push_create_error(const json_t *req_id, const json_t *params);

/* v1.0 inline push config on message/send -> callback URL ("" when absent). */
json_t *w29_adapter_0e0bda_inline_push_url(const json_t *params);

/* Forwarded-admission LRU: order[] + input_id -> {seen, order}. */
json_t *w29_adapter_0e0bda_note_forwarded(const json_t *order, const json_t *input_id, const json_t *max_entries);

/* Drop a refused fresh id from the LRU: order[] + input_id -> {order}. */
json_t *w29_adapter_0e0bda_forget_forwarded(const json_t *order, const json_t *input_id);

/* {"name": "a2a:<chat_id>", "type": "dm"} */
json_t *w29_adapter_0e0bda_chat_info(const json_t *chat_id);

#ifdef __cplusplus
}
#endif
#endif
