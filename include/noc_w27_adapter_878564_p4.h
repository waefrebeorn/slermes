/*
 * noc_w27_adapter_878564_p4.h - C surface for upstream
 * plugins/platforms/photon/adapter.py (gate w27_adapter_878564_p4).
 *
 * Gate 4 of 5: IDS/KEYS, sidecar error typing, retry classification, the module
 * constants and timestamp shaping.  The env, the base adapter's retryable
 * verdict and the clock are seams.
 */
#ifndef NOC_W27_ADAPTER_878564_P4_H
#define NOC_W27_ADAPTER_878564_P4_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PhotonAdapter._normalize_chat_key: "any;-;+1555…" → "+1555…", else verbatim.
 * Caller frees. */
char *w27_adapter_878564_p4_normalize_chat_key(const char *chat_id);

/* _sent_text_key: the normalized key, or NULL for a falsy chat id. */
char *w27_adapter_878564_p4_sent_text_key(const char *chat_id);

/* _SEND_DM_GUID_RE full match — the adapter-native target ref. */
int w27_adapter_878564_p4_is_dm_guid(const char *text);

/* _sidecar_error_from_response: {"path","status_code","error","error_class",
 * "retryable","message"} with the canonical target_not_allowed text.  `data`
 * NULL means "parse `text` as JSON, else {}". */
json_t *w27_adapter_878564_p4_sidecar_error_from_response(const char *path, double status_code,
                                                          const char *text,
                                                          const json_t *data);

/* PhotonAdapter._is_retryable_error with the base adapter's verdict as a seam. */
int w27_adapter_878564_p4_retryable_error(const char *error, int base_retryable);

/* _is_permanent_sidecar_failure: auth_or_config / target_not_allowed with an
 * explicit retryable=false in raw_response. */
int w27_adapter_878564_p4_is_permanent_sidecar_failure(const json_t *result);

/* _send_retry_is_final: exactly the permanent verdict. */
int w27_adapter_878564_p4_send_retry_is_final(const json_t *result);

/* The module/class budget and wording constants, as an object. */
json_t *w27_adapter_878564_p4_constants(void);

/* _parse_timestamp: ISO-8601 in, `.isoformat()` text out; `now_iso` is the
 * clock seam used for empty/invalid input → {"iso","used_now"}. */
json_t *w27_adapter_878564_p4_parse_timestamp(const char *ts, const char *now_iso);

#ifdef __cplusplus
}
#endif
#endif
