/*
 * noc_w27_adapter_74f618_p11.h - C surface for upstream
 * plugins/platforms/slack/adapter.py (gate w27_adapter_74f618_p11).
 *
 * Gate 11: the out-of-process sender's payload shape, the bot-token list,
 * the bounded standalone DM cache, the connected predicate and the expired
 * picker notice.
 */
#ifndef NOC_W27_ADAPTER_74F618_P11_H
#define NOC_W27_ADAPTER_74F618_P11_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req: {"chat_id": str, "text": any, "unfurl_kwargs": obj|null,
 *       "thread_id": str|null} -> obj */
json_t *w27_p11_post_kwargs(const json_t *req);

/* req: {"raw_token": str?, "saved": obj|null} -> [str] (CSV first, then file order) */
json_t *w27_p11_bot_tokens(const json_t *req);

/* req: {"text": str} -> str (_standalone_format_mrkdwn == format_message) */
json_t *w27_p11_format_mrkdwn(const json_t *req);

/* req: {} -> str (the shipped English picker-expired notice) */
json_t *w27_p11_model_picker_notice(const json_t *req);

/* req: {"mapping": obj, "max_size": int?} -> obj (oldest dropped while over cap) */
json_t *w27_p11_trim_dm_cache(const json_t *req);

/* req: {"token": str|null} -> bool (every required env non-blank) */
json_t *w27_p11_is_connected(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W27_ADAPTER_74F618_P11_H */
