#ifndef NOC_W5_BASE_C4CC9E_H
#define NOC_W5_BASE_C4CC9E_H

#include <stddef.h>
struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w5_base_c4cc9e_get_hostname @ providers/base.py:ProviderProfile.get_hostname */
char *w5_base_c4cc9e_get_hostname(const char *hostname, const char *base_url);

/* PoP: w5_base_c4cc9e_get_max_tokens @ providers/base.py:ProviderProfile.get_max_tokens
 * 64-bit values travel as decimal strings; is_null mirrors Python None. */
struct json_t *w5_base_c4cc9e_get_max_tokens(int is_null, const char *decimal);

/* PoP: w5_base_c4cc9e_api_kwargs @ providers/base.py:ProviderProfile.build_api_kwargs_extras */
struct json_t *w5_base_c4cc9e_api_kwargs(void);

/* PoP: w5_base_c4cc9e_prepare_messages @ providers/base.py:ProviderProfile.prepare_messages */
struct json_t *w5_base_c4cc9e_prepare_messages(const struct json_t *messages);

/* PoP: w5_base_c4cc9e_supported_reasoning_efforts @ providers/base.py:ProviderProfile.supported_reasoning_efforts */
struct json_t *w5_base_c4cc9e_supported_reasoning_efforts(void);

/* PoP: w5_base_c4cc9e_default_vision_model @ providers/base.py:ProviderProfile.default_vision_model */
struct json_t *w5_base_c4cc9e_default_vision_model(void);

/* PoP: w5_base_c4cc9e_resolve_aux_model @ providers/base.py:ProviderProfile.resolve_aux_model */
struct json_t *w5_base_c4cc9e_resolve_aux_model(void);

/* PoP: w5_base_c4cc9e_build_extra_body @ providers/base.py:ProviderProfile.build_extra_body */
struct json_t *w5_base_c4cc9e_build_extra_body(void);

/* PoP: w5_base_c4cc9e_default_reasoning_config @ providers/base.py:ProviderProfile.default_reasoning_config */
struct json_t *w5_base_c4cc9e_default_reasoning_config(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W5_BASE_C4CC9E_H */
