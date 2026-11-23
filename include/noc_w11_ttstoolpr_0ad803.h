#ifndef NOC_W11_TTSTOOLPR_0AD803_H
#define NOC_W11_TTSTOOLPR_0AD803_H

/* C API for the tools/tts_tool_providers.py port (gate w11_ttstoolpr_0ad803). */
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w11_ttstoolpr_0ad803_config_bool @ tools/tts_tool_providers.py:_config_bool */
json_t *w11_ttstoolpr_0ad803_config_bool(const json_t *value, int default_val);
/* PoP: w11_ttstoolpr_0ad803_response_format @ tools/tts_tool_providers.py:_tts_response_format_from_path */
json_t *w11_ttstoolpr_0ad803_response_format(const char *output_path);
/* PoP: w11_ttstoolpr_0ad803_clamped_number @ tools/tts_tool_providers.py:_clamped_number
 * cast is "int" or "float"; lo/hi come from the fixture as JSON numbers. */
json_t *w11_ttstoolpr_0ad803_clamped_number(const json_t *raw, const char *cast,
                                            double lo, double hi);
/* PoP: w11_ttstoolpr_0ad803_gemini_audio_tags @ tools/tts_tool_providers.py:_gemini_audio_tags_enabled */
json_t *w11_ttstoolpr_0ad803_gemini_audio_tags(const json_t *gemini_config, const char *model);
/* PoP: w11_ttstoolpr_0ad803_compose_gemini_prompt @ tools/tts_tool_providers.py:_compose_gemini_tts_prompt
 * persona_prompt is supplied by the fixture (no file reads); pass "" for the no-persona path. */
json_t *w11_ttstoolpr_0ad803_compose_gemini_prompt(const char *text, const char *persona_prompt);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W11_TTSTOOLPR_0AD803_H */
