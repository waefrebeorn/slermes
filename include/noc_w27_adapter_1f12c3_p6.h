/*
 * noc_w27_adapter_1f12c3_p6.h - C surface for upstream
 * plugins/platforms/discord/adapter.py (gate w27_adapter_1f12c3_p6).
 *
 * voice/config readers and the fail-closed warning (adapter.py:3480-4100).
 * PORTED (pure): the voice_fx defaults merge, the discord.<key> int reader and
 * its two call sites, the stored voice/playback timeout limits, the mixer-active
 * test, the allowed-channel intersection and the fail-closed default warning
 * decision.
 * SEAM: read_raw_config() (the parsed discord: block is the parameter), the
 * adapter's live dicts/sets and the logger.
 */
#ifndef NOC_W27_ADAPTER_1F12C3_P6_H
#define NOC_W27_ADAPTER_1F12C3_P6_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

json_t *w27_adapter_1f12c3_p6_load_voice_fx_config(const json_t *fx);
json_t *w27_adapter_1f12c3_p6_load_discord_int_config(const json_t *cfg, const json_t *key, const json_t *dflt, const json_t *minimum);
json_t *w27_adapter_1f12c3_p6_load_voice_timeout(const json_t *cfg);
json_t *w27_adapter_1f12c3_p6_load_playback_timeout(const json_t *cfg);
json_t *w27_adapter_1f12c3_p6_voice_timeout_limit(const json_t *stored);
json_t *w27_adapter_1f12c3_p6_playback_timeout_limit(const json_t *stored);
json_t *w27_adapter_1f12c3_p6_voice_mixer_active(const json_t *mixers, const json_t *guild_id);
json_t *w27_adapter_1f12c3_p6_discord_channel_ids_allowed(const json_t *channel_ids, const json_t *allowed);
json_t *w27_adapter_1f12c3_p6_warn_if_fail_closed_default(const json_t *state);

#ifdef __cplusplus
}
#endif
#endif

