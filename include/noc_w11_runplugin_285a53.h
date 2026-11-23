#ifndef NOC_W11_RUNPLUGIN_285A53_H
#define NOC_W11_RUNPLUGIN_285A53_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w11_runplugin_285a53_count @ gateway/run_plugin_rewire.py:_count_adapters
 * Returns len(adapters) for the launch profile, or the named secondary.
 * Mirrors `or {}` fallbacks. */
int w11_runplugin_285a53_count(const struct json_t *adapters,
                               const struct json_t *profile_adapters,
                               const char *profile_name);

/* PoP: w11_runplugin_285a53_rewire_count @ gateway/run_plugin_rewire.py:GatewayPluginRewireMixin._rewire_plugin_handlers
 * Counts the adapters whose rewire_plugin_handlers() succeeds; platform names
 * in `fail` raise and are skipped. Same adapter selection as the live method. */
int w11_runplugin_285a53_rewire_count(const struct json_t *adapters,
                                      const struct json_t *profile_adapters,
                                      const char *profile_name,
                                      const struct json_t *fail);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W11_RUNPLUGIN_285A53_H */
