/*
 * noc_w27_adapter_74f618_p10.h - C surface for upstream
 * plugins/platforms/slack/adapter.py (gate w27_adapter_74f618_p10).
 *
 * Gate 10: the admission policy — mention requirement, free-response and
 * allowed/ignored channel sets, "addressed to someone else", reply_expected,
 * the Slack-CDN allowlist and the rehydration key. No API, no state: every
 * predicate is a function of the configured sets.
 */
#ifndef NOC_W27_ADAPTER_74F618_P10_H
#define NOC_W27_ADAPTER_74F618_P10_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req: {"extra": obj|null, "env": str|null} -> bool */
json_t *w27_p10_require_mention(const json_t *req);

/* req: {"extra": obj|null, "key": str, "env": str|null, "strip": bool?} -> bool */
json_t *w27_p10_extra_or_env_flag(const json_t *req);

/* req: {"text": str?, "self_uids": [str]} -> bool */
json_t *w27_p10_addressed_other_user(const json_t *req);

/* req: {"channel_id": str?, "require_mention_channels": [str],
 *       "free_response_channels": [str], "require_mention": bool?} -> bool */
json_t *w27_p10_is_free_channel(const json_t *req);

/* req: {"routing_text": str?, "bot_uid": str|null, "channel_id": str,
 *       "addressed": bool?, "opens_own_session": bool?,
 *       "channel_thread_followup": bool?, "thread_followup_silence": bool?,
 *       "free_channel": bool?} -> true|false|null */
json_t *w27_p10_reply_expected(const json_t *req);

/* req: {"text": str?, "self_uids": [str]} -> bool */
json_t *w27_p10_message_mentions_self(const json_t *req);

/* req: {"raw": any, "coerce_scalar": bool?} -> [str] (sorted) */
json_t *w27_p10_extra_or_env_channel_set(const json_t *req);

/* req: {"channel_id": str?, "ignored": [str], "allowed": [str]} -> bool */
json_t *w27_p10_slash_channel_gated(const json_t *req);

/* req: {"url": str?} -> bool */
json_t *w27_p10_is_slack_cdn_url(const json_t *req);

/* req: {"channel_id": str?, "thread_ts": str?, "user_id": str?,
 *       "team_id": str?, "per_user": bool?} -> str */
json_t *w27_p10_thread_rehydration_key(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W27_ADAPTER_74F618_P10_H */
