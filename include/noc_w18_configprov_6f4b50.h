#ifndef NOC_W18_CONFIGPROV_6F4B50_H
#define NOC_W18_CONFIGPROV_6F4B50_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w18_configprov_6f4b50_entries_for @ hermes_cli/config_provider_env.py:_entries_for
 * Entries for the auth_type="api_key" profiles' env vars, minus `skip`; the
 * first profile to declare a name wins.  `profiles` is an array of objects
 * (auth_type, env_vars, display_name, name, signup_url); `skip` is an array of
 * strings.  Returns a NEW json_t* object (caller frees). */
struct json_t *w18_configprov_6f4b50_entries_for(const struct json_t *profiles,
                                                 const struct json_t *skip);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W18_CONFIGPROV_6F4B50_H */
