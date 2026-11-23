#ifndef NOC_W15_VOICEMODE_7AD9C5_H
#define NOC_W15_VOICEMODE_7AD9C5_H
/*
 * Public C API for the port of tools/voice_mode_transcript.py
 * (gate w15_voicemode_7ad9c5). All exported symbols are gate-prefixed.
 *
 * PoP: w15_voicemode_7ad9c5_is_whisper_hallucination @ tools/voice_mode_transcript.py:is_whisper_hallucination
 * PoP: w15_voicemode_7ad9c5_normalize_for_echo_compare @ tools/voice_mode_transcript.py:_normalize_for_echo_compare
 * PoP: w15_voicemode_7ad9c5_is_tts_echo @ tools/voice_mode_transcript.py:is_tts_echo
 * PoP: w15_voicemode_7ad9c5_is_voice_stop_phrase @ tools/voice_mode_transcript.py:is_voice_stop_phrase
 * PoP: w15_voicemode_7ad9c5_voice_stop_hint @ tools/voice_mode_transcript.py:voice_stop_hint
 *
 * Config/plugin reads in the live module (_voice_config, _load_voice_stop_phrases)
 * are resolved-fact seams here: the caller injects the already-resolved stop
 * phrases as a JSON array of strings; the literal strings and logic are real.
 */
#include "hermes_json.h"

int  w15_voicemode_7ad9c5_is_whisper_hallucination(const char *transcript);
char *w15_voicemode_7ad9c5_normalize_for_echo_compare(const char *text);
int  w15_voicemode_7ad9c5_is_tts_echo(const char *transcript, const char *spoken_text, double threshold);
int  w15_voicemode_7ad9c5_is_voice_stop_phrase(const char *transcript, const json_t *stop_phrases);
char *w15_voicemode_7ad9c5_voice_stop_hint(const json_t *stop_phrases);

#endif /* NOC_W15_VOICEMODE_7AD9C5_H */
