#ifndef NOC_W17_PLUGINKEYL_D5F537_H
#define NOC_W17_PLUGINKEYL_D5F537_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w17_pluginkeyl_d5f537_local @ gateway/plugin_keyless_inject.py:_local
 * Mirrors: source is not None and source.platform == Platform.LOCAL and
 * authorize_local_source(runner, source) is True. Args are the three facts. */
int w17_pluginkeyl_d5f537_local(int has_source, int platform_is_local, int authorized);

/* PoP: w17_pluginkeyl_d5f537_launched_by_classic_cli @ gateway/plugin_keyless_inject.py:_launched_by_classic_cli
 * Mirrors: policy_for_source may raise AuthorityRefusal (refusal=1) -> False;
 * else policy is not None and policy.source == "cli". */
int w17_pluginkeyl_d5f537_launched_by_classic_cli(int refusal, int has_policy, const char *policy_source);

/* PoP: w17_pluginkeyl_d5f537_resolve @ gateway/plugin_keyless_inject.py:resolve
 * Reads the fixture object (plugin_home, serving/authority, own_sessions,
 * other_sessions, calling_session_key, plugin_id). Returns a freshly allocated
 * JSON object {"authority":"own","route":<str>} on success, or a JSON null. */
struct json_t *w17_pluginkeyl_d5f537_resolve(const struct json_t *fixture);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W17_PLUGINKEYL_D5F537_H */
