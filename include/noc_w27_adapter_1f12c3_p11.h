/*
 * noc_w27_adapter_1f12c3_p11.h - C surface for upstream
 * plugins/platforms/discord/adapter.py (gate w27_adapter_1f12c3_p11).
 *
 * bot-tag debounce and mention parsing (adapter.py:4961-5040).
 * PORTED (pure): the bot-tag debounce window record and its continuation test,
 * the free-response channel set, and the raw mention-id parser with the
 * self-mention test.
 * SEAM: time.monotonic/time.time (shimmed), is_automated_message, the adapter's
 * debounce dict, os.getenv and the discord.py client user.
 */
#ifndef NOC_W27_ADAPTER_1F12C3_P11_H
#define NOC_W27_ADAPTER_1F12C3_P11_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

json_t *w27_adapter_1f12c3_p11_record_bot_tag_debounce(const json_t *delay, const json_t *automated, const json_t *mentioned, const json_t *now, const json_t *split_delay);
json_t *w27_adapter_1f12c3_p11_is_bot_tag_debounce_continuation(const json_t *delay, const json_t *automated, const json_t *until, const json_t *now, const json_t *window);
/* `_gate_env("DISCORD_FREE_RESPONSE_CHANNELS")` is a lookup by a module constant:
 * an environment variable under any other name is not this setting, so the NAME is
 * part of the decision, not just the value. */
json_t *w27_adapter_1f12c3_p11_discord_free_response_channels(const json_t *extra,
                                                              const json_t *env_name,
                                                              const json_t *env);
json_t *w27_adapter_1f12c3_p11_raw_mentioned_user_ids(const json_t *content);
json_t *w27_adapter_1f12c3_p11_self_is_raw_mentioned(const json_t *client_user_id, const json_t *content);

#ifdef __cplusplus
}
#endif
#endif

