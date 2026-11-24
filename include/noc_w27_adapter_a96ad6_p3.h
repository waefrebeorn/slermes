/*
 * noc_w27_adapter_a96ad6_p3.h - C surface for slice 3 of upstream
 * plugins/platforms/telegram/adapter.py (send/typing thread routing).
 *
 * Each symbol takes the JSON-lines request object and returns the
 * Python-equivalent JSON value of the same-named upstream function applied
 * to the keys named after its parameters. See adapter.c for the PoP map.
 */
#ifndef NOC_W27_ADAPTER_A96AD6_P3_H
#define NOC_W27_ADAPTER_A96AD6_P3_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

json_t *w27_adapter_a96ad6_p3_reply_to_message_id_for_send(const json_t *req);
json_t *w27_adapter_a96ad6_p3_thread_kwargs_for_send(const json_t *req);
json_t *w27_adapter_a96ad6_p3_direct_topic_kwargs(const json_t *req);
json_t *w27_adapter_a96ad6_p3_thread_kwargs_for_draft(const json_t *req);
json_t *w27_adapter_a96ad6_p3_message_thread_id_for_send(const json_t *req);
json_t *w27_adapter_a96ad6_p3_message_thread_id_for_typing(const json_t *req);

/* Dispatch by request "func"; returns "UNKNOWN_FUNC" when absent. */
json_t *w27_adapter_a96ad6_p3_dispatch(const char *func, const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W27_ADAPTER_A96AD6_P3_H */
