#ifndef NOC_W18_GATEWAYBUN_C3A152_H
#define NOC_W18_GATEWAYBUN_C3A152_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w18_gatewaybun_c3a152_env_pairs @ hermes_cli/gateway_bundled_env.py:bundled_resource_env_pairs
 * env/prior are injected seams for os.environ and the on-disk unit; result is
 * a JSON array of [name, value] pairs in BUNDLED_RESOURCE_ENV_VARS order. */
struct json_t *w18_gatewaybun_c3a152_env_pairs(const struct json_t *prior,
                                                const struct json_t *env);

/* PoP: w18_gatewaybun_c3a152_systemd_env_line @ hermes_cli/gateway_bundled_env.py:systemd_env_line */
struct json_t *w18_gatewaybun_c3a152_systemd_env_line(const char *name, const char *value);

/* PoP: w18_gatewaybun_c3a152_systemd_bundled_env_block @ hermes_cli/gateway_bundled_env.py:systemd_bundled_env_block
 * prior/env are injected seams for _unit_environment_value / os.environ. */
struct json_t *w18_gatewaybun_c3a152_systemd_bundled_env_block(const struct json_t *prior,
                                                                const struct json_t *env);

/* PoP: w18_gatewaybun_c3a152_launchd_bundled_env_xml @ hermes_cli/gateway_bundled_env.py:launchd_bundled_env_xml
 * prior/env are injected seams for _prior_launchd_env / os.environ. */
struct json_t *w18_gatewaybun_c3a152_launchd_bundled_env_xml(const struct json_t *prior,
                                                              const struct json_t *env);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W18_GATEWAYBUN_C3A152_H */
