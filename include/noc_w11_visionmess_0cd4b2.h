#ifndef NOC_W11_VISIONMESS_0CD4B2_H
#define NOC_W11_VISIONMESS_0CD4B2_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w11_visionmess_0cd4b2_is_image_part @ agent/vision_message_prep.py:_is_image_part */
int w11_visionmess_0cd4b2_is_image_part(const json_t *part);
/* PoP: w11_visionmess_0cd4b2_salvage_text_parts @ agent/vision_message_prep.py:_salvage_text_parts */
json_t *w11_visionmess_0cd4b2_salvage_text_parts(const json_t *content, int any_dict_text);
/* PoP: w11_visionmess_0cd4b2_content_has_image_parts @ agent/vision_message_prep.py:VisionMessagePrepMixin._content_has_image_parts */
int w11_visionmess_0cd4b2_content_has_image_parts(const json_t *content);
/* PoP: w11_visionmess_0cd4b2_provider_model_key @ agent/vision_message_prep.py:_provider_model_key
 * Returns [provider.strip().lower(), model.strip()]. */
json_t *w11_visionmess_0cd4b2_provider_model_key(const char *provider, const char *model);
/* PoP: w11_visionmess_0cd4b2_anthropic_preserve_dots @ agent/vision_message_prep.py:VisionMessagePrepMixin._anthropic_preserve_dots
 * base_url_hostname / base_url_host_matches are ported verbatim from utils/urls.py. */
int w11_visionmess_0cd4b2_anthropic_preserve_dots(const char *provider, const char *base_url);
/* PoP: w11_visionmess_0cd4b2_is_qwen_portal @ agent/vision_message_prep.py:VisionMessagePrepMixin._is_qwen_portal */
int w11_visionmess_0cd4b2_is_qwen_portal(const char *base_url_lower);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W11_VISIONMESS_0CD4B2_H */
