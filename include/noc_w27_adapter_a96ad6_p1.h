/*
 * noc_w27_adapter_a96ad6_p1.h - C surface for slice 1 of upstream
 * plugins/platforms/telegram/adapter.py (label/notification/flood/chat-type values).
 *
 * Each symbol takes the JSON-lines request object and returns the
 * Python-equivalent JSON value of the same-named upstream function applied
 * to the keys named after its parameters. See adapter.c for the PoP map.
 */
#ifndef NOC_W27_ADAPTER_A96AD6_P1_H
#define NOC_W27_ADAPTER_A96AD6_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

json_t *w27_adapter_a96ad6_p1_bold_label_html(const json_t *req);
json_t *w27_adapter_a96ad6_p1_flood_cap_result(const json_t *req);
json_t *w27_adapter_a96ad6_p1_env_float_clamped(const json_t *req);
json_t *w27_adapter_a96ad6_p1_notification_kwargs(const json_t *req);
json_t *w27_adapter_a96ad6_p1_normalize_chat_type(const json_t *req);
json_t *w27_adapter_a96ad6_p1_gateway_chat_type(const json_t *req);

/* Dispatch by request "func"; returns "UNKNOWN_FUNC" when absent. */
json_t *w27_adapter_a96ad6_p1_dispatch(const char *func, const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W27_ADAPTER_A96AD6_P1_H */
