/*
 * noc_w27_adapter_a96ad6_p11.h - C surface for slice 11 of upstream
 * plugins/platforms/telegram/adapter.py (group/thread/entity/bot-mention parsing).
 *
 * Each symbol takes the JSON-lines request object and returns the
 * Python-equivalent JSON value of the same-named upstream function applied
 * to the keys named after its parameters. See adapter.c for the PoP map.
 */
#ifndef NOC_W27_ADAPTER_A96AD6_P11_H
#define NOC_W27_ADAPTER_A96AD6_P11_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

json_t *w27_adapter_a96ad6_p11_is_group_chat(const json_t *req);
json_t *w27_adapter_a96ad6_p11_effective_message_thread_id(const json_t *req);
json_t *w27_adapter_a96ad6_p11_is_reply_to_bot(const json_t *req);
json_t *w27_adapter_a96ad6_p11_entity_type(const json_t *req);
json_t *w27_adapter_a96ad6_p11_entity_span(const json_t *req);
json_t *w27_adapter_a96ad6_p11_telegram_entity_text(const json_t *req);
json_t *w27_adapter_a96ad6_p11_extract_bot_mention_usernames(const json_t *req);

/* Dispatch by request "func"; returns "UNKNOWN_FUNC" when absent. */
json_t *w27_adapter_a96ad6_p11_dispatch(const char *func, const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W27_ADAPTER_A96AD6_P11_H */
