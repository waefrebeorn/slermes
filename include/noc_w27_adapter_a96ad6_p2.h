/*
 * noc_w27_adapter_a96ad6_p2.h - C surface for slice 2 of upstream
 * plugins/platforms/telegram/adapter.py (metadata + DM-topic addressing).
 *
 * Each symbol takes the JSON-lines request object and returns the
 * Python-equivalent JSON value of the same-named upstream function applied
 * to the keys named after its parameters. See adapter.c for the PoP map.
 */
#ifndef NOC_W27_ADAPTER_A96AD6_P2_H
#define NOC_W27_ADAPTER_A96AD6_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

json_t *w27_adapter_a96ad6_p2_metadata_thread_id(const json_t *req);
json_t *w27_adapter_a96ad6_p2_metadata_direct_messages_topic_id(const json_t *req);
json_t *w27_adapter_a96ad6_p2_metadata_reply_to_message_id(const json_t *req);
json_t *w27_adapter_a96ad6_p2_dm_topic_fallback(const json_t *req);
json_t *w27_adapter_a96ad6_p2_dm_topic_missing_anchor_error(const json_t *req);
json_t *w27_adapter_a96ad6_p2_is_private_dm_topic_send(const json_t *req);

/* Dispatch by request "func"; returns "UNKNOWN_FUNC" when absent. */
json_t *w27_adapter_a96ad6_p2_dispatch(const char *func, const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W27_ADAPTER_A96AD6_P2_H */
