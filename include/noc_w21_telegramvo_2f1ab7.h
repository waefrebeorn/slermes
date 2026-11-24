/*
 * noc_w21_telegramvo_2f1ab7.h -- public C API for the port of upstream Hermes
 *   plugins/platforms/telegram/telegram_voice_mention.py (gate w21_telegramvo_2f1ab7).
 *
 * Ported: the whole spoken-wake-word admission ladder of admit_media (message
 * gate, caption, mention patterns, the voice_mention_patterns opt-in, media
 * size, the fetch, and `clips or None`) and the attach-or-download decision of
 * cache_inbound_voice. The network/STT seams (fetch_telegram_file,
 * cache_audio_from_bytes_async, spoken_mention_clips) are supplied as `world`.
 */
#ifndef NOC_W21_TELEGRAMVO_2F1AB7_H
#define NOC_W21_TELEGRAMVO_2F1AB7_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* admit_media(adapter, msg) as a pure ladder. `world`:
 *   should_process    -- adapter._should_process_message(msg) (the text gate)
 *   has_voice         -- getattr(msg, "voice", None) is not None
 *   caption           -- getattr(msg, "caption", None); truthiness decides
 *   mention_patterns  -- bool(adapter._mention_patterns)
 *   gate_extra        -- adapter.config.extra["voice_mention_patterns"] raw value
 *                        (is_truthy_value semantics; absent/null = off)
 *   spoken_ok         -- adapter._should_process_message(msg, spoken_mention=True)
 *   media_allowed     -- adapter._telegram_media_size_allowed(voice, ...)[0]
 *   fetch_ok          -- False when fetch_telegram_file/cache raised
 *   clips             -- spoken_mention_clips(...) result (array of paths)
 * Returns [] (passes as usual), null (dropped/observed), or the clip array. */
json_t *w21_telegramvo_2f1ab7_admit_media(const json_t *world);

/* cache_inbound_voice(adapter, msg, event, clips): `world.clips` is the clip
 * list. Returns {"status":"download",...} when it falls back to
 * adapter._cache_inbound_av, else the event fields it sets. */
json_t *w21_telegramvo_2f1ab7_cache_inbound_voice(const json_t *world);

/* Probe of the harness environment: {"sler":bool,"home":bool,"same":bool}. */
json_t *w21_telegramvo_2f1ab7_env_probe(void);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W21_TELEGRAMVO_2F1AB7_H */
