#ifndef NOC_W10_TTSTOOLDE_5650BD_H
#define NOC_W10_TTSTOOLDE_5650BD_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Pure text/container helpers of tools/tts_tool_delivery.py. */
json_t *w10_ttstoolde_5650bd_section(const json_t *tts_config, const char *key);
json_t *w10_ttstoolde_5650bd_positive_int(const json_t *value);
json_t *w10_ttstoolde_5650bd_resolve_max_text_length(const char *provider, const json_t *tts_config);
json_t *w10_ttstoolde_5650bd_resolve_audio_delivery_profile(const char *platform, const json_t *tts_config);
json_t *w10_ttstoolde_5650bd_pack_under_cap(const json_t *pieces, int max_chars, int slice_oversized);
json_t *w10_ttstoolde_5650bd_split_oversized_sentence(const char *sentence, int max_chars);
json_t *w10_ttstoolde_5650bd_split_text_for_tts(const char *text, int max_chars);
json_t *w10_ttstoolde_5650bd_wav_sidecar_path(const char *output_path);
json_t *w10_ttstoolde_5650bd_wrap_pcm_as_wav(const json_t *pcm_bytes, int sample_rate, int channels, int sample_width);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W10_TTSTOOLDE_5650BD_H */
