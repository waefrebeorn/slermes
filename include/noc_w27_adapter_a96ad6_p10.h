/*
 * noc_w27_adapter_a96ad6_p10.h - C surface for slice 10 of upstream
 * plugins/platforms/telegram/adapter.py (extra/env coercion + chat identity strings).
 *
 * Each symbol takes the JSON-lines request object and returns the
 * Python-equivalent JSON value of the same-named upstream function applied
 * to the keys named after its parameters. See adapter.c for the PoP map.
 */
#ifndef NOC_W27_ADAPTER_A96AD6_P10_H
#define NOC_W27_ADAPTER_A96AD6_P10_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

json_t *w27_adapter_a96ad6_p10_is_transient_typing_error(const json_t *req);
json_t *w27_adapter_a96ad6_p10_extra_bool(const json_t *req);
json_t *w27_adapter_a96ad6_p10_extra_str_set(const json_t *req);
json_t *w27_adapter_a96ad6_p10_chat_type_str(const json_t *req);
json_t *w27_adapter_a96ad6_p10_chat_id_str(const json_t *req);
json_t *w27_adapter_a96ad6_p10_topic_id_or_general(const json_t *req);

/* Dispatch by request "func"; returns "UNKNOWN_FUNC" when absent. */
json_t *w27_adapter_a96ad6_p10_dispatch(const char *func, const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W27_ADAPTER_A96AD6_P10_H */
