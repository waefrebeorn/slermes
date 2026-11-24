/*
 * noc_w26_telegramau_5c2119.h - C surface for upstream
 * plugins/platforms/telegram/telegram_audio_meta.py
 * (gate w26_telegramau_5c2119).
 *
 * The media facts a Telegram media send carries: whole-second audio duration,
 * the music-player tag fields of sendAudio, and video geometry plus the frame
 * seek a thumbnail is cut at.  wave/mutagen/ffprobe/ffmpeg are seams: every
 * entry point takes the value a probe would have returned.
 */
#ifndef NOC_W26_TELEGRAMAU_5C2119_H
#define NOC_W26_TELEGRAMAU_5C2119_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Telegram's own limit on a sendAudio performer/title. */
#define W26_TELEGRAMAU_5C2119_TAG_MAX_CHARS 256

/* {"value": <any>} -> int | null   (telegram_audio_meta._coerce_duration_seconds) */
json_t *w26_telegramau_5c2119_coerce_duration(const json_t *in);

/* {"blob": <ffprobe -of json output>} -> {"performer"?: str, "title"?: str}
 * (the tag half of telegram_audio_meta.audio_player_fields) */
json_t *w26_telegramau_5c2119_audio_tags(const json_t *in);

/* {"blob": <ffprobe -of json output>} -> bool  (has_cover inside audio_player_fields) */
json_t *w26_telegramau_5c2119_has_cover(const json_t *in);

/* {"blob": <ffprobe -of json output>} -> {"width","height"[,"duration"]} | {}
 * (telegram_audio_meta._probe_video_geometry) */
json_t *w26_telegramau_5c2119_video_geometry(const json_t *in);

/* {"duration": int|null} -> int   (telegram_audio_meta._video_thumbnail_jpeg's seek) */
json_t *w26_telegramau_5c2119_video_thumb_seek(const json_t *in);

/* {"rate": int|null, "nframes": int} -> int | null   (the wave branch of
 * telegram_audio_meta._probe_voice_duration_seconds) */
json_t *w26_telegramau_5c2119_wav_duration(const json_t *in);

#ifdef __cplusplus
}
#endif
#endif
