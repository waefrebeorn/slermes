/*
 * noc_w27_adapter_a96ad6_p13.h - C surface for slice 13 of upstream
 * plugins/platforms/telegram/adapter.py (batching delay, extensions, dm-topic config).
 *
 * Each symbol takes the JSON-lines request object and returns the
 * Python-equivalent JSON value of the same-named upstream function applied
 * to the keys named after its parameters. See adapter.c for the PoP map.
 */
#ifndef NOC_W27_ADAPTER_A96AD6_P13_H
#define NOC_W27_ADAPTER_A96AD6_P13_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

json_t *w27_adapter_a96ad6_p13_text_batch_delay_for(const json_t *req);
json_t *w27_adapter_a96ad6_p13_ext_from_path(const json_t *req);
json_t *w27_adapter_a96ad6_p13_get_dm_topic_info(const json_t *req);
json_t *w27_adapter_a96ad6_p13_resolve_topic_binding(const json_t *req);
json_t *w27_adapter_a96ad6_p13_group_observe_channel_prompt(const json_t *req);

/* Dispatch by request "func"; returns "UNKNOWN_FUNC" when absent. */
json_t *w27_adapter_a96ad6_p13_dispatch(const char *func, const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W27_ADAPTER_A96AD6_P13_H */
