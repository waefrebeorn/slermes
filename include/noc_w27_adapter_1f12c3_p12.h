/*
 * noc_w27_adapter_1f12c3_p12.h - C surface for upstream
 * plugins/platforms/discord/adapter.py (gate w27_adapter_1f12c3_p12).
 *
 * channel keys and history backfill (adapter.py:5041-5100).
 * PORTED (pure): the bots-require-inline-mention policy, the channel key set
 * (id / name / #name / parent id / parent name / #parent name), the
 * thread-mention and history-backfill flags and the history backfill limit.
 * SEAM: os.getenv (value is the parameter) and the discord.py channel objects
 * (only id / name / parent_id / parent.name are read).
 */
#ifndef NOC_W27_ADAPTER_1F12C3_P12_H
#define NOC_W27_ADAPTER_1F12C3_P12_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

json_t *w27_adapter_1f12c3_p12_discord_bots_require_inline_mention(const json_t *extra, const json_t *env);
json_t *w27_adapter_1f12c3_p12_discord_channel_keys_from_channel(const json_t *channel, const json_t *parent_channel_id);
json_t *w27_adapter_1f12c3_p12_discord_channel_keys(const json_t *channel, const json_t *parent_channel_id);
json_t *w27_adapter_1f12c3_p12_discord_thread_require_mention(const json_t *extra, const json_t *env);
json_t *w27_adapter_1f12c3_p12_discord_history_backfill(const json_t *extra, const json_t *env);
json_t *w27_adapter_1f12c3_p12_discord_history_backfill_limit(const json_t *extra, const json_t *env);

#ifdef __cplusplus
}
#endif
#endif

