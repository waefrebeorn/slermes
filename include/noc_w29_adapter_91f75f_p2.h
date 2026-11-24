/*
 * noc_w29_adapter_91f75f_p2.h - C surface for upstream
 * plugins/platforms/dingtalk/adapter.py, gate p2 (payload shaping, retry
 * math, credential keys).
 *
 * Every HTTP post, SDK call and clock read stays in the caller; what is
 * ported is the exact value each of them is built from -- the markdown
 * payload the session webhook receives, the text payload cron delivery uses,
 * the composed image reply, the "no local upload" refusal, the reconnect
 * backoff/circuit-breaker decision, the session-webhook expiry margin, the
 * (client_id, client_secret) pair, the nested allowlist lookup and the
 * dedup key the SDK log guard collapses records under.
 *
 * Seams: get_scoped_secret (env rung arrives as env_id/env_secret),
 * datetime.now (now_ms), logging's record.getMessage (msg arrives formatted)
 * and the transports themselves.
 */
#ifndef NOC_W29_ADAPTER_91F75F_P2_H
#define NOC_W29_ADAPTER_91F75F_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* content (str) -> {"msgtype":"markdown","markdown":{"title":"Hermes",
 * "text": normalize(content[:20000])}} — the session-webhook body. */
json_t *w29_adapter_91f75f_webhook_payload(const json_t *content);

/* message (any value) -> {"msgtype":"text","text":{"content": str(message)}}
 * — the standalone/cron robot-webhook body. */
json_t *w29_adapter_91f75f_standalone_payload(const json_t *message);

/* caption (any), image_url (str) -> "![image](url)" or "caption\n\n![image](url)". */
json_t *w29_adapter_91f75f_image_content(const json_t *caption,
                                         const json_t *image_url);

/* kind: "image" | "file" -> the _NO_LOCAL_UPLOAD refusal text, else null. */
json_t *w29_adapter_91f75f_no_local_upload(const json_t *kind);

/* consecutive (errors seen so far), same_type (this error repeats the last
 * type), backoff_idx -> {"consecutive_same_error","delay","tripped",
 * "backoff_idx"}: the sleep and the advanced index for the next pass. */
json_t *w29_adapter_91f75f_reconnect_plan(const json_t *consecutive,
                                          const json_t *same_type,
                                          const json_t *backoff_idx);

/* info: {"webhook": str, "expired_ms": num|null}, now_ms -> info when still
 * usable, null once within 5 minutes of (or past) expiry. */
json_t *w29_adapter_91f75f_webhook_valid(const json_t *info,
                                         const json_t *now_ms);

/* extra (config.extra), env_id, env_secret (the scoped-secret seam) ->
 * [client_id, client_secret] with extra winning over env. */
json_t *w29_adapter_91f75f_credentials(const json_t *extra,
                                       const json_t *env_id,
                                       const json_t *env_secret);

/* all(_credentials(...)) -> bool. */
json_t *w29_adapter_91f75f_is_connected(const json_t *extra,
                                        const json_t *env_id,
                                        const json_t *env_secret);

/* yaml_cfg, dingtalk_cfg -> extra.allowed_users from the first holder that has
 * a non-null one (this block, gateway.platforms.dingtalk, platforms.dingtalk),
 * else null. */
json_t *w29_adapter_91f75f_nested_allowed_users(const json_t *yaml_cfg,
                                                const json_t *dingtalk_cfg);

/* level, msg, exc_type -> [levelno, msg[:500], exc type name | null], the
 * _SdkLogGuard repeat-collapse key. */
json_t *w29_adapter_91f75f_sdk_log_key(const json_t *level,
                                       const json_t *msg,
                                       const json_t *exc_type);

#ifdef __cplusplus
}
#endif
#endif
