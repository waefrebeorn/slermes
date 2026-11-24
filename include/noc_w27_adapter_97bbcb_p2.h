/*
 * noc_w27_adapter_97bbcb_p2.h - C surface for upstream
 * plugins/platforms/google_chat/adapter.py (gate w27_adapter_97bbcb_p2).
 *
 * Gate 2 of 4: the adapter's decision predicates and configuration gate -
 * the SSRF host allow-list, the error-text redactions, the retry
 * classification of a failed API call, project/subscription validation,
 * resource-name validation, the HTTP-event bearer identity check and the
 * str/int setting coercion.  No call is made: values in, verdicts out.
 */
#ifndef NOC_W27_ADAPTER_97BBCB_P2_H
#define NOC_W27_ADAPTER_97BBCB_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _is_google_owned_host: req {"url": str} -> bool */
json_t *w27_gchat_p2_is_google_owned_host(const json_t *req);

/* _redact_sensitive: req {"text": str} -> str */
json_t *w27_gchat_p2_redact_sensitive(const json_t *req);

/* _http_status + _is_retryable_error: req {"status": int|null, "message": str}
 * -> {"status": int|null, "retryable": bool} */
json_t *w27_gchat_p2_retry_class(const json_t *req);

/* GoogleChatAdapter._validate_config: req {"extra": obj}
 * -> {"ok": true, "project_id": str, "subscription": str|null}
 *  | {"ok": false, "error": str} */
json_t *w27_gchat_p2_validate_config(const json_t *req);

/* module _validate_config: req {"extra": obj} -> bool */
json_t *w27_gchat_p2_plugin_validate_config(const json_t *req);

/* _is_connected: req {"enabled": bool, "extra": obj} -> bool */
json_t *w27_gchat_p2_is_connected(const json_t *req);

/* _GCHAT_CHAT_ID_RE.match: req {"chat_id": str} -> bool */
json_t *w27_gchat_p2_space_id_ok(const json_t *req);

/* _GCHAT_THREAD_ID_RE.match: req {"thread_id": str} -> bool */
json_t *w27_gchat_p2_thread_id_ok(const json_t *req);

/* verify_http_event_request (id-token seam fed as claims):
 * req {"auth_header": str, "audience": str, "sa_emails": str,
 *      "claims": obj|null, "claims_error": bool?}
 * -> {"ok": bool, "reason": str} */
json_t *w27_gchat_p2_verify_http_event(const json_t *req);

/* _int_setting: req {"extra": obj, "key": str, "env_value": str, "default": int} -> int */
json_t *w27_gchat_p2_int_setting(const json_t *req);

/* _str_setting: req {"extra": obj, "key": str, "env_value": str, "fallback": str} -> str */
json_t *w27_gchat_p2_str_setting(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W27_ADAPTER_97BBCB_P2_H */
