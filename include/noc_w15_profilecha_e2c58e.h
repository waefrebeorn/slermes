#ifndef NOC_W15_PROFILECHA_E2C58E_H
#define NOC_W15_PROFILECHA_E2C58E_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_profilecha_e2c58e_platform_env_prefixes @ hermes_cli/profile_channels.py:platform_env_prefixes */
struct json_t *w15_profilecha_e2c58e_platform_env_prefixes(const char *platform_id);
/* PoP: w15_profilecha_e2c58e_env_key_of_line @ hermes_cli/profile_channels.py:_env_key_of_line */
struct json_t *w15_profilecha_e2c58e_env_key_of_line(const char *line);
/* PoP: w15_profilecha_e2c58e_explicit_enabled @ hermes_cli/profile_channels.py:_explicit_enabled */
struct json_t *w15_profilecha_e2c58e_explicit_enabled(const struct json_t *raw, const char *pid);
/* PoP: w15_profilecha_e2c58e_channel_config_paths @ hermes_cli/profile_channels.py:_channel_config_paths */
struct json_t *w15_profilecha_e2c58e_channel_config_paths(const struct json_t *raw, const struct json_t *platforms);
/* PoP: w15_profilecha_e2c58e_shared_credential_warning @ hermes_cli/profile_channels.py:shared_credential_warning */
struct json_t *w15_profilecha_e2c58e_shared_credential_warning(const char *profile,
                                                              const struct json_t *platforms,
                                                              const char *source);
/* PoP: w15_profilecha_e2c58e_format_stripped_notice @ hermes_cli/profile_channels.py:format_stripped_notice */
struct json_t *w15_profilecha_e2c58e_format_stripped_notice(const char *profile,
                                                            const struct json_t *platforms,
                                                            const char *clone_flag);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_PROFILECHA_E2C58E_H */
