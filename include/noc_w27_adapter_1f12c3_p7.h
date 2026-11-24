/*
 * noc_w27_adapter_1f12c3_p7.h - C surface for upstream
 * plugins/platforms/discord/adapter.py (gate w27_adapter_1f12c3_p7).
 *
 * authorization predicate and channel shape readers (adapter.py:3995-5345).
 * PORTED (pure): the message authorization predicate (_is_allowed_user with its
 * wildcard / channel-scoped / pairing / role branches), the guild-scoped member
 * role test, the interaction guild id, the forum-parent / parent-id / effective-
 * topic readers, the attachment type classifier and the auto thread name.
 * SEAM: discord.py guild/member/channel objects (modelled by their ids), the
 * pairing store, _refresh_live_gates and the t() catalog default title.
 */
#ifndef NOC_W27_ADAPTER_1F12C3_P7_H
#define NOC_W27_ADAPTER_1F12C3_P7_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

json_t *w27_adapter_1f12c3_p7_is_allowed_user(const json_t *req);
json_t *w27_adapter_1f12c3_p7_guild_member_has_role(const json_t *guild, const json_t *user_id, const json_t *allowed_roles);
json_t *w27_adapter_1f12c3_p7_interaction_guild_id(const json_t *interaction);
json_t *w27_adapter_1f12c3_p7_is_forum_parent(const json_t *channel);
json_t *w27_adapter_1f12c3_p7_get_parent_channel_id(const json_t *channel);
json_t *w27_adapter_1f12c3_p7_get_effective_topic(const json_t *channel, const json_t *is_thread);
json_t *w27_adapter_1f12c3_p7_attachment_message_type(const json_t *att);
json_t *w27_adapter_1f12c3_p7_derive_auto_thread_name(const json_t *content, const json_t *default_name);

#ifdef __cplusplus
}
#endif
#endif

