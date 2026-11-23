#ifndef NOC_CFGWENV_H
#define NOC_CFGWENV_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: cfgwenv_only_keys       @ gateway/config_env.py:_only_keys
 * PoP: cfgwenv_platform_layers @ gateway/config_env.py:_platform_layers
 * PoP: cfgwenv_platform_rank   @ gateway/config_env.py:_platform_rank
 *
 * Faithful ports of the pure helpers in gateway/config_env.py:
 *   _only_keys(section, names)       - copy of section's entries named in names.
 *   _platform_layers(yaml_cfg,names) - (top, nested) loader inputs as a 2-array.
 *   _platform_rank(entry)            - enum position of a platform name (or
 *                                      len(members) when it is not a member).
 *
 * `names` / `members` are JSON arrays of strings injected as data. Each function
 * returns a NEWLY allocated json_t the caller owns and must json_free().
 */
struct json_t *cfgwenv_only_keys(const struct json_t *section, const struct json_t *names);

struct json_t *cfgwenv_platform_layers(const struct json_t *yaml_cfg, const struct json_t *names);

struct json_t *cfgwenv_platform_rank(const char *name, const struct json_t *members);

#ifdef __cplusplus
}
#endif

#endif /* NOC_CFGWENV_H */
