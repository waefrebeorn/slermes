#ifndef NOC_W11_WEBTOOLST_DAE278_H
#define NOC_W11_WEBTOOLST_DAE278_H

/* C API for the tools/web_tools_truncate.py port (gate w11_webtoolst_dae278). */
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w11_webtoolst_dae278_clamp_char_limit @ tools/web_tools_truncate.py:_clamp_char_limit
 * value is a JSON scalar; invalid inputs return the string "__ERR__" (oracle maps
 * Python TypeError/ValueError to that sentinel). */
json_t *w11_webtoolst_dae278_clamp_char_limit(const json_t *value);

/* PoP: w11_webtoolst_dae278_clamp_or_default @ tools/web_tools_truncate.py:_clamp_or_default */
json_t *w11_webtoolst_dae278_clamp_or_default(const json_t *value);

/* PoP: w11_webtoolst_dae278_effective_char_limit @ tools/web_tools_truncate.py:_effective_char_limit
 * config_value is the config's extract_char_limit (used when char_limit is null). */
json_t *w11_webtoolst_dae278_effective_char_limit(const json_t *char_limit,
                                                  const json_t *config_value);

/* PoP: w11_webtoolst_dae278_convert_base64_images @ tools/web_tools_truncate.py:convert_base64_images_to_links */
json_t *w11_webtoolst_dae278_convert_base64_images(const char *text);

/* PoP: w11_webtoolst_dae278_trim_results @ tools/web_tools_truncate.py:_trim_results */
json_t *w11_webtoolst_dae278_trim_results(const json_t *results);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W11_WEBTOOLST_DAE278_H */
