/* Public C API for the tools/tts_tool_local.py parity port. */
#ifndef NOC_W7_TTSTOOLLOC_4467F6_H
#define NOC_W7_TTSTOOLLOC_4467F6_H
#include "hermes_json.h"

json_t *w7_ttstoolloc_4467f6_constants(void);
json_t *w7_ttstoolloc_4467f6_voice_default(json_t *voice);
/* entries: ordered [[key, value], ...]; returns {value, cache, loaded}. */
json_t *w7_ttstoolloc_4467f6_cache_get_or_load(json_t *entries, const char *key,
                                              json_t *load_value, long max_entries);
json_t *w7_ttstoolloc_4467f6_piper_cache_key(const char *model_path, int use_cuda);
/* raw int64 is a DECIMAL STRING (libjson numbers are double). */
json_t *w7_ttstoolloc_4467f6_piper_speaker_id(json_t *raw);
json_t *w7_ttstoolloc_4467f6_has_advanced_knobs(json_t *keys);
#endif /* NOC_W7_TTSTOOLLOC_4467F6_H */
