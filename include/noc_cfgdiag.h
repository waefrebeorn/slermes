#ifndef NOC_CFGDIAG_H
#define NOC_CFGDIAG_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: cfgdiag_config_check_diagnostics @ hermes_cli/config_check_diagnostics.py:_validate_voice
 * Voice portion: returns a JSON array of "message. hint." diagnostic strings. */
struct json_t *cfgdiag_voice(const struct json_t *config);

/* PoP: cfgdiag_config_check_diagnostics @ hermes_cli/config_check_diagnostics.py:config_check_diagnostics
 * Bundled-platform disabled/credentials portion, with the platform manifest
 * scan, disabled set and get_env_value injected as explicit seams:
 *   manifests: [{"name": <platform>, "manifest_name": <str|null>, "required": [env,..]}]
 *   disabled:  [name, ..]
 *   env:       {env_name: value, ..}
 * Returns a JSON array of diagnostic strings. */
struct json_t *cfgdiag_disabled_platforms(const struct json_t *manifests,
                                          const struct json_t *disabled,
                                          const struct json_t *env);

#ifdef __cplusplus
}
#endif

#endif /* NOC_CFGDIAG_H */
