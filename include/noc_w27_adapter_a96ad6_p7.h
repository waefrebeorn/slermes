/*
 * noc_w27_adapter_a96ad6_p7.h - C surface for slice 7 of upstream
 * plugins/platforms/telegram/adapter.py (reply routing + timed-out/final-send policy).
 *
 * Each symbol takes the JSON-lines request object and returns the
 * Python-equivalent JSON value of the same-named upstream function applied
 * to the keys named after its parameters. See adapter.c for the PoP map.
 */
#ifndef NOC_W27_ADAPTER_A96AD6_P7_H
#define NOC_W27_ADAPTER_A96AD6_P7_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

json_t *w27_adapter_a96ad6_p7_is_timed_out(const json_t *req);
json_t *w27_adapter_a96ad6_p7_should_thread_reply(const json_t *req);
json_t *w27_adapter_a96ad6_p7_chunk_reply_routing(const json_t *req);
json_t *w27_adapter_a96ad6_p7_compute_single_send_routing(const json_t *req);
json_t *w27_adapter_a96ad6_p7_send_retry_is_final(const json_t *req);

/* Dispatch by request "func"; returns "UNKNOWN_FUNC" when absent. */
json_t *w27_adapter_a96ad6_p7_dispatch(const char *func, const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W27_ADAPTER_A96AD6_P7_H */
