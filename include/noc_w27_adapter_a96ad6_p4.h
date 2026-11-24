/*
 * noc_w27_adapter_a96ad6_p4.h - C surface for slice 4 of upstream
 * plugins/platforms/telegram/adapter.py (error classification (thread/anchor/conflict/auth)).
 *
 * Each symbol takes the JSON-lines request object and returns the
 * Python-equivalent JSON value of the same-named upstream function applied
 * to the keys named after its parameters. See adapter.c for the PoP map.
 */
#ifndef NOC_W27_ADAPTER_A96AD6_P4_H
#define NOC_W27_ADAPTER_A96AD6_P4_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

json_t *w27_adapter_a96ad6_p4_is_thread_not_found_error(const json_t *req);
json_t *w27_adapter_a96ad6_p4_is_bad_request_error(const json_t *req);
json_t *w27_adapter_a96ad6_p4_is_reply_anchor_missing_error(const json_t *req);
json_t *w27_adapter_a96ad6_p4_should_retry_without_dm_topic_reply_anchor(const json_t *req);
json_t *w27_adapter_a96ad6_p4_looks_like_polling_conflict(const json_t *req);
json_t *w27_adapter_a96ad6_p4_looks_like_auth_error(const json_t *req);

/* Dispatch by request "func"; returns "UNKNOWN_FUNC" when absent. */
json_t *w27_adapter_a96ad6_p4_dispatch(const char *func, const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W27_ADAPTER_A96AD6_P4_H */
