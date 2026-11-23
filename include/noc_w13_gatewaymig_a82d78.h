#ifndef NOC_W13_GATEWAYMIG_A82D78_H
#define NOC_W13_GATEWAYMIG_A82D78_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w13_gatewaymig_a82d78_constants @ hermes_cli/gateway_migrate.py (module constants) */
struct json_t *w13_gatewaymig_a82d78_constants(void);

/* PoP: w13_gatewaymig_a82d78_service_label @ hermes_cli/gateway_migrate.py:_service_label */
struct json_t *w13_gatewaymig_a82d78_service_label(const char *kind, int system);

/* PoP: w13_gatewaymig_a82d78_remove_verb @ hermes_cli/gateway_migrate.py:_remove_verb */
struct json_t *w13_gatewaymig_a82d78_remove_verb(const char *kind);

/* PoP: w13_gatewaymig_a82d78_service_dict @ hermes_cli/gateway_migrate.py:_service_dict */
struct json_t *w13_gatewaymig_a82d78_service_dict(const char *kind, int system);

/* PoP: w13_gatewaymig_a82d78_credential_key_names @ hermes_cli/gateway_migrate.py:_credential_key_names
 * env_keys maps env-key-name -> platform id; injected credential_env_keys() seam. */
struct json_t *w13_gatewaymig_a82d78_credential_key_names(const char *platform_value,
                                                         const struct json_t *env_keys);

/* PoP: w13_gatewaymig_a82d78_listener_url @ hermes_cli/gateway_migrate.py:_listener_url
 * extra_host / extra_port come from the default config; format_url_host is identity (seam). */
struct json_t *w13_gatewaymig_a82d78_listener_url(const char *platform_value, const char *profile,
                                                  const char *extra_host, const char *extra_port);

/* PoP: w13_gatewaymig_a82d78_manifest_secondaries @ hermes_cli/gateway_migrate.py:_manifest_secondaries */
struct json_t *w13_gatewaymig_a82d78_manifest_secondaries(const struct json_t *manifest);

/* PoP: w13_gatewaymig_a82d78_recorded_services @ hermes_cli/gateway_migrate.py:_recorded_services */
struct json_t *w13_gatewaymig_a82d78_recorded_services(const struct json_t *rec);

/* PoP: w13_gatewaymig_a82d78_recorded_run_as_user @ hermes_cli/gateway_migrate.py:_recorded_run_as_user */
struct json_t *w13_gatewaymig_a82d78_recorded_run_as_user(const struct json_t *rec);

/* PoP: w13_gatewaymig_a82d78_target_from_manifest @ hermes_cli/gateway_migrate.py:_target_from_manifest */
struct json_t *w13_gatewaymig_a82d78_target_from_manifest(const struct json_t *manifest);

/* PoP: w13_gatewaymig_a82d78_manifest_not_yet_served @ hermes_cli/gateway_migrate.py:_manifest_not_yet_served
 * parked = list of home paths for which profile_is_parked() is True (seam). */
struct json_t *w13_gatewaymig_a82d78_manifest_not_yet_served(const struct json_t *manifest,
                                                             const struct json_t *live_served,
                                                             const struct json_t *parked);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_GATEWAYMIG_A82D78_H */
