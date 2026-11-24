/*
 * noc_w27_adapter_e9cb6b_p3.h - C surface for the p3 slice of upstream
 * plugins/platforms/matrix/adapter.py (gate w27_adapter_e9cb6b_p3).
 *
 * Outbound payload shaping: the approval tier/reaction tables, picker choice
 * flattening, reply/thread relation metadata, outbound mention protection,
 * extraction and link injection, the bot-mention predicate, localpart helpers,
 * mention stripping, mxc->http resolution, link-URL sanitising and the image
 * markdown stripper.  Sending, uploading and translating stay seamed.
 */
#ifndef NOC_W27_ADAPTER_E9CB6B_P3_H
#define NOC_W27_ADAPTER_E9CB6B_P3_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* (choices, smart_denied) -> "always" | "session" | "once". */
json_t *w27_adapter_exec_approval_tier(const json_t *choices, const json_t *smart_denied);

/* tier -> emoji, null when the tier is unknown. */
json_t *w27_adapter_approval_reaction(const json_t *tier);

/* emoji -> "once"|"session"|"always"|"deny", null when unmapped. */
json_t *w27_adapter_approval_action(const json_t *emoji);

/* providers -> [[model_id, slug, name], ...] capped at the reaction count. */
json_t *w27_adapter_flat_picker_choices(const json_t *providers);

/* choices -> {emoji: value} capped at the choice-reaction count. */
json_t *w27_adapter_choice_picker_emoji(const json_t *choices);

/* (content, reply_to, thread_id) -> content with m.relates_to applied. */
json_t *w27_adapter_apply_relation_metadata(const json_t *content, const json_t *reply_to,
                                            const json_t *thread_id);

/* text -> {"protected", "placeholders"} with fenced/inline code and links masked. */
json_t *w27_adapter_protect_mention_regions(const json_t *text);

/* text -> deduped @user:server mentions outside protected regions. */
json_t *w27_adapter_extract_outbound_mentions(const json_t *text);

/* text -> @room present outside protected regions. */
json_t *w27_adapter_has_outbound_room_mention(const json_t *text);

/* text -> mentions wrapped in matrix.to markdown links outside code/link spans. */
json_t *w27_adapter_inject_outbound_mention_links(const json_t *text);

/* (user_id, body, formatted_body, m.mentions.user_ids) -> bot mentioned. */
json_t *w27_adapter_is_bot_mentioned(const json_t *user_id, const json_t *body,
                                     const json_t *formatted_body, const json_t *mention_user_ids);

/* "@bot:server" -> "bot"; "" when there is no server part. */
json_t *w27_adapter_user_localpart(const json_t *user_id);

/* (user_id, body) -> body with explicit @user:server / @localpart tokens removed. */
json_t *w27_adapter_strip_mention(const json_t *user_id, const json_t *body);

/* (homeserver, mxc_url) -> https media download URL; unchanged when not mxc://. */
json_t *w27_adapter_mxc_to_http(const json_t *homeserver, const json_t *mxc_url);

/* url -> stripped URL with dangerous schemes blanked and quotes escaped. */
json_t *w27_adapter_sanitize_link_url(const json_t *url);

/* content -> image markdown replaced with its URL (media is uploaded separately). */
json_t *w27_adapter_format_message(const json_t *content);

#ifdef __cplusplus
}
#endif
#endif
