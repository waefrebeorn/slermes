/*
 * noc_w32_audio_a418b2_p1.h - C surface for upstream
 * hermes_cli/web_routers/audio.py (gate w32_audio_a418b2_p1).
 *
 * The audio *payload* half of the dashboard audio router: which file suffix a
 * MIME type maps to, what POST /api/audio/transcribe decides about an uploaded
 * data URL (validation order, strict base64, size budget, transcript shaping)
 * and what POST /api/audio/speak hands back as a base64 data URL.
 *
 * Every I/O seam is a fixture input: the temp file write/unlink, the provider
 * round-trip (transcribe_recording / text_to_speech_tool), os.path.isfile, the
 * upload size budget (``max_bytes``) and credential redaction (identity on the
 * plain messages these fixtures carry).
 */
#ifndef NOC_W32_AUDIO_A418B2_P1_H
#define NOC_W32_AUDIO_A418B2_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The transcription upload ceiling upstream hardcodes. */
#define W32_AUDIO_A418B2_MAX_UPLOAD_BYTES (25u * 1024u * 1024u)

/* mime_type: <any> -> ".mp3" / ".wav" / ... / ".webm" (the suffix a temp file
 * is created with).  None, "" and any other falsy value fall back to .webm. */
json_t *w32_audio_a418b2_p1_extension_for_mime(const json_t *mime_type);

/* req: {data_url, mime_type, result, max_bytes?} -> the route's answer:
 *   {"status": 200, "ok": true, "transcript": ..., "provider": ...}
 *   {"status": 400|413|500, "detail": "..."}
 *   {"status": 0, "error": "AttributeError"} for a value the route would have
 *   raised on (an out-of-contract, post-validation payload shape).
 * ``result`` is the transcription provider's answer (success/transcript/
 * provider/error); ``max_bytes`` overrides the upload budget for boundary
 * cases and defaults to W32_AUDIO_A418B2_MAX_UPLOAD_BYTES. */
json_t *w32_audio_a418b2_p1_transcribe(const json_t *req);

/* req: {text, tts_result, file_exists, audio_b64} -> the route's answer:
 * {"status":200,"ok":true,"data_url":...,"mime_type":...,"provider":...}
 * or {"status":400|500,"detail":"..."}.  ``tts_result`` is what the TTS tool
 * returned (a JSON string or an already-parsed object, carrying file_path),
 * ``file_exists`` is the os.path.isfile seam and ``audio_b64`` the synthesized
 * file's bytes. */
json_t *w32_audio_a418b2_p1_speak(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
