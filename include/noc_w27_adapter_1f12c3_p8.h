/*
 * noc_w27_adapter_1f12c3_p8.h - C surface for upstream
 * plugins/platforms/discord/adapter.py (gate w27_adapter_1f12c3_p8).
 *
 * flag readers (adapter.py:2381, 4810-4870).
 * PORTED (pure): the missed-message-backfill channel set (extra/YAML, env, or
 * the allowed + free-response union), the extra->env->default flag resolver with
 * its three wrappers, the 32 MiB attachment cap reader and the voice-note
 * attachment test.
 * SEAM: os.getenv (value is the parameter), the adapter config extra dict and
 * the discord.py attachment (only its marker/duration/waveform are read).
 */
#ifndef NOC_W27_ADAPTER_1F12C3_P8_H
#define NOC_W27_ADAPTER_1F12C3_P8_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

json_t *w27_adapter_1f12c3_p8_missed_message_backfill_channels(const json_t *extra, const json_t *env, const json_t *allowed_channels, const json_t *free_response);
json_t *w27_adapter_1f12c3_p8_extra_or_env_flag(const json_t *extra, const json_t *key, const json_t *env_default, const json_t *truthy, const json_t *env);
json_t *w27_adapter_1f12c3_p8_discord_require_mention(const json_t *extra, const json_t *env);
json_t *w27_adapter_1f12c3_p8_discord_free_response_auto_thread(const json_t *extra, const json_t *env);
json_t *w27_adapter_1f12c3_p8_discord_max_attachment_bytes(const json_t *extra, const json_t *env);
json_t *w27_adapter_1f12c3_p8_is_discord_voice_message_attachment(const json_t *marker, const json_t *duration, const json_t *waveform);

#ifdef __cplusplus
}
#endif
#endif

