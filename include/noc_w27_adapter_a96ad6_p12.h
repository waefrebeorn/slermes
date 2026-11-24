/*
 * noc_w27_adapter_a96ad6_p12.h - C surface for slice 12 of upstream
 * plugins/platforms/telegram/adapter.py (mention matching + sender identity).
 *
 * Each symbol takes the JSON-lines request object and returns the
 * Python-equivalent JSON value of the same-named upstream function applied
 * to the keys named after its parameters. See adapter.c for the PoP map.
 */
#ifndef NOC_W27_ADAPTER_A96AD6_P12_H
#define NOC_W27_ADAPTER_A96AD6_P12_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

json_t *w27_adapter_a96ad6_p12_message_mentions_bot(const json_t *req);
json_t *w27_adapter_a96ad6_p12_sender_is_other_bot(const json_t *req);
json_t *w27_adapter_a96ad6_p12_bot_sender_suppressed(const json_t *req);
json_t *w27_adapter_a96ad6_p12_media_message_type(const json_t *req);
json_t *w27_adapter_a96ad6_p12_append_observed_note(const json_t *req);
json_t *w27_adapter_a96ad6_p12_is_own_message(const json_t *req);

/* Dispatch by request "func"; returns "UNKNOWN_FUNC" when absent. */
json_t *w27_adapter_a96ad6_p12_dispatch(const char *func, const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W27_ADAPTER_A96AD6_P12_H */
