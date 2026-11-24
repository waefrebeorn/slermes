/*
 * noc_w29_adapter_91f75f_p1.h - C surface for upstream
 * plugins/platforms/dingtalk/adapter.py, gate p1 (text + predicates).
 *
 * The Stream Mode adapter is a socket, an SDK, an event loop and a webhook
 * cache.  None of that is here: this gate owns the value core the inbound
 * path decides with -- how a CSV/JSON-list setting becomes a set, how a
 * markdown body is repaired for the DingTalk renderer, which truthiness rule
 * decides "groups need a mention", who passes the user allowlist, whether a
 * group message clears the mention gate, whether an SDK exception is the
 * websockets incompatibility, and how a chat is labelled.
 *
 * Seams: the scoped-secret/env ladder (a null setting reads as its
 * extra_or_secret default), and re.Pattern.search (one hit flag per pattern).
 */
#ifndef NOC_W29_ADAPTER_91F75F_P1_H
#define NOC_W29_ADAPTER_91F75F_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The module budgets this adapter is tuned with, as an object. */
json_t *w29_adapter_91f75f_constants(void);

/* raw: any JSON value (JSON-list string, CSV string, list, null, bool, int)
 * -> the stripped, non-empty, de-duplicated, sorted items. */
json_t *w29_adapter_91f75f_csv_set(const json_t *raw);

/* text -> the body with a blank line inserted before a numbered list and
 * every ``` fence dedented. */
json_t *w29_adapter_91f75f_normalize_markdown(const json_t *text);

/* configured: the resolved require_mention value (null => unset) -> bool for
 * "group chats need an explicit trigger". */
json_t *w29_adapter_91f75f_require_mention(const json_t *configured);

/* allowed_users (raw setting), sender_id, sender_staff_id -> bool.  An empty
 * or "*" allowlist admits everyone; ids compare case-insensitively. */
json_t *w29_adapter_91f75f_is_user_allowed(const json_t *allowed_raw,
                                           const json_t *sender_id,
                                           const json_t *sender_staff_id);

/* message: {is_group, chat_id, text, is_in_at_list, allowed_chats,
 * free_response_chats, require_mention, pattern_hits} -> bool.  DMs always
 * pass; allowed_chats is a hard gate; then free_response / require_mention off
 * / @mentioned / wake-word.  pattern_hits is the regex seam (one flag per
 * compiled pattern). */
json_t *w29_adapter_91f75f_should_process_message(const json_t *req);

/* chain: [{"type","msg","te"?,"cause":idx|null,"context":idx|null}, ...] — the
 * exception and its __cause__/__context__ links, starting at index 0 -> bool.
 * Walks at most 4 hops, exactly like the live loop. */
json_t *w29_adapter_91f75f_is_sdk_incompat(const json_t *chain);

/* chat_id -> {"name": chat_id, "type": "group" | "dm"}. */
json_t *w29_adapter_91f75f_chat_info(const json_t *chat_id);

#ifdef __cplusplus
}
#endif
#endif
