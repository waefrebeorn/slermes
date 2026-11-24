/*
 * noc_w32_audio_a418b2_p2.h - C surface for upstream
 * hermes_cli/web_routers/audio.py (gate w32_audio_a418b2_p2).
 *
 * The *contract* half of the dashboard audio router: the ElevenLabs voice list
 * projection (filter, label, case-insensitive sort), the GPT-Live SDP session
 * handshake verdict and the TTS lease toggle's answer, including the
 * error-signal forms their bodies carry.
 *
 * Seams fed as fixture inputs: the urllib fetch and its HTTP status, the
 * webrtc session call and its ValueError/RuntimeError signal, the
 * acquire/release lease call, the profile scope and credential redaction
 * (identity on the plain messages these fixtures carry).
 */
#ifndef NOC_W32_AUDIO_A418B2_P2_H
#define NOC_W32_AUDIO_A418B2_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* voice: {"name"?, "voice_id"?, "category"?} -> the dropdown label,
 * "Name (category)" when a category is present, "Name" otherwise. */
json_t *w32_audio_a418b2_p2_voice_label(const json_t *voice);

/* req: {payload: {"voices": [...]}, http_status?} -> the route's answer:
 *   {"status":200,"available":true,"voices":[{"voice_id","name","label"}...]}
 *   {"status":200,"available":false,"voices":[],"error":"unauthorized"} (401/403)
 *   {"status":502,"detail":"Could not load ElevenLabs voices"}
 * ``payload`` is the parsed ElevenLabs /v1/voices body; ``http_status`` makes
 * the fetch seam raise urllib's HTTPError with that code. */
json_t *w32_audio_a418b2_p2_voices(const json_t *req);

/* req: {sdp, error_kind?, error_text?, result} -> the GPT-Live session answer:
 *   {"status":200,"ok":true, <result keys>}
 *   {"status":400,"detail":"An SDP offer is required"}
 *   {"status":503,"detail":<error_text>}   (error_kind "value")
 *   {"status":502,"detail":<error_text>}   (error_kind "runtime") */
json_t *w32_audio_a418b2_p2_sdp_session(const json_t *req);

/* req: {lease, active?, result, error_text?} -> the TTS lease answer:
 *   {"status":200,"ok":true,"lease":...,"active":..., <result keys>}
 *   {"status":400,"detail":"lease is required"}
 * ``result: null`` is the warm-up-failure seam: the answer becomes
 * {"leases":null,"action":"error","error":<error_text>}. */
json_t *w32_audio_a418b2_p2_lease(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
