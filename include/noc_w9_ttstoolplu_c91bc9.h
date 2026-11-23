#ifndef NOC_W9_TTSTOOLPLU_C91BC9_H
#define NOC_W9_TTSTOOLPLU_C91BC9_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w9_ttstoolplu_c91bc9_route_key @ tools/tts_tool_plugins.py:_plugin_route_key */
struct json_t *w9_ttstoolplu_c91bc9_route_key(const char *provider, const struct json_t *builtins,
                             const char *nous, int has_command);
/* PoP: w9_ttstoolplu_c91bc9_voice_kwargs @ tools/tts_tool_plugins.py:_plugin_voice_kwargs */
struct json_t *w9_ttstoolplu_c91bc9_voice_kwargs(const struct json_t *config);
/* PoP: w9_ttstoolplu_c91bc9_voice_compatible @ tools/tts_tool_plugins.py:_plugin_provider_is_voice_compatible */
int w9_ttstoolplu_c91bc9_voice_compatible(const char *provider, const struct json_t *builtins, int available);
/* PoP: w9_ttstoolplu_c91bc9_is_available @ tools/tts_tool_plugins.py:_plugin_provider_is_available */
int w9_ttstoolplu_c91bc9_is_available(const char *provider, int available);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W9_TTSTOOLPLU_C91BC9_H */
