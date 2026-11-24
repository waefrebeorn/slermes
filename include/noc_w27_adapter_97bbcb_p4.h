/*
 * noc_w27_adapter_97bbcb_p4.h - C surface for upstream
 * plugins/platforms/google_chat/adapter.py (gate w27_adapter_97bbcb_p4).
 *
 * Gate 4 of 4: Markdown -> Google Chat dialect conversion (fenced/inline code
 * shielding, headers, bold/strike, Slack-style links, invisible-codepoint
 * stripping), the retry/reconnect backoff arithmetic, and the small routing
 * policies (bot-space candidates, SA credential classification, unadmitted
 * release, outbound thread tracking, credential eviction, loop liveness).
 */
#ifndef NOC_W27_ADAPTER_97BBCB_P4_H
#define NOC_W27_ADAPTER_97BBCB_P4_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* format_message: req {"content": str} -> str */
json_t *w27_gchat_p4_format_message(const json_t *req);

/* _call_with_retry arithmetic: req {"randoms": [num], "failures": int}
 * -> {"waits": [num], "delays": [num], "exhausted": bool} */
json_t *w27_gchat_p4_retry_plan(const json_t *req);

/* _run_supervisor backoff: req {"attempt": int} -> num */
json_t *w27_gchat_p4_reconnect_delay(const json_t *req);

/* _run_supervisor full jitter: req {"attempt": int, "rand": num} -> num */
json_t *w27_gchat_p4_reconnect_jitter(const json_t *req);

/* _resolve_bot_user_id candidates: req {"home": str|null, "bootstrap": str} -> [str] */
json_t *w27_gchat_p4_bot_spaces(const json_t *req);

/* _load_sa_credentials_from classification: req {"sa_value": str|null,
 *   "exists": bool, "inline_valid": bool, "file_valid": bool,
 *   "multiplex": bool, "env_has_sa": bool, "has_google_auth": bool,
 *   "adc_raises": bool} -> {"source": "inline"|"file"|"adc"|null,
 *                           "kind": "creds"|"inline_invalid"|"not_found"
 *                                  |"file_invalid"|"adc_foreign"|"adc_no_auth"|"adc_failed"} */
json_t *w27_gchat_p4_sa_source_kind(const json_t *req);

/* _release_unadmitted: req {"msg": obj, "envelope": obj}
 * -> {"dedup": str, "decr": {"chat": str, "thread": str}|null} */
json_t *w27_gchat_p4_release_unadmitted(const json_t *req);

/* _track_outbound_thread: req {"chat_id": str, "resp": obj}
 * -> {"chat": str, "thread": str}|null */
json_t *w27_gchat_p4_outbound_thread_incr(const json_t *req);

/* _invalidate_user_creds: req {"identity": str|null} -> "none"|"legacy"|"email" */
json_t *w27_gchat_p4_invalidate_identity(const json_t *req);

/* _loop_accepts_callbacks: req {"present": bool, "closed": bool} -> bool */
json_t *w27_gchat_p4_loop_accepts(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W27_ADAPTER_97BBCB_P4_H */
