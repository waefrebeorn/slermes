/*
 * noc_w27_adapter_1f12c3_p10.h - C surface for upstream
 * plugins/platforms/discord/adapter.py (gate w27_adapter_1f12c3_p10).
 *
 * identity sets and bot-tag timing (adapter.py:4929-4960).
 * PORTED (pure): the resolved-username allowlist set, the two allow-all-user
 * readers, the allow-bots policy string, the bot-tag debounce key and the
 * debounce window arithmetic.
 * SEAM: os.getenv (value is the parameter), the connect-time snapshot and the
 * adapter's username-resolved id set.
 */
#ifndef NOC_W27_ADAPTER_1F12C3_P10_H
#define NOC_W27_ADAPTER_1F12C3_P10_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

json_t *w27_adapter_1f12c3_p10_resolved_allowlist_user_ids(const json_t *ids);
json_t *w27_adapter_1f12c3_p10_discord_allow_all_users(const json_t *env_val, const json_t *extra);
json_t *w27_adapter_1f12c3_p10_gateway_allow_all_users(const json_t *env_val);
json_t *w27_adapter_1f12c3_p10_get_allow_bots(const json_t *env_val, const json_t *extra);
json_t *w27_adapter_1f12c3_p10_bot_tag_debounce_key(const json_t *channel_id, const json_t *author_id);
json_t *w27_adapter_1f12c3_p10_bot_tag_window_seconds(const json_t *delay, const json_t *split_delay);

#ifdef __cplusplus
}
#endif
#endif

