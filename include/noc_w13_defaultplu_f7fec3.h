#ifndef NOC_W13_DEFAULTPLU_F7FEC3_H
#define NOC_W13_DEFAULTPLU_F7FEC3_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w13_defaultplu_f7fec3_attempt @ hermes_cli/default_plugins.py:_attempt
 * Failure-stamp name "default-<entry.name>". */
struct json_t *w13_defaultplu_f7fec3_attempt(const struct json_t *name);

/* PoP: w13_defaultplu_f7fec3_fetch_policy @ hermes_cli/default_plugins.py:fetch_policy
 * "denied" / "allowed" / "unreadable". env_value and security are explicit seams
 * for HERMES_DISABLE_LAZY_INSTALLS and read_yaml_layers()["security"]; readable
 * mirrors whether that config read raised. */
struct json_t *w13_defaultplu_f7fec3_fetch_policy(const struct json_t *env_value,
        const struct json_t *security, int readable);

/* PoP: w13_defaultplu_f7fec3_fetch_allowed @ hermes_cli/default_plugins.py:fetch_allowed */
struct json_t *w13_defaultplu_f7fec3_fetch_allowed(const struct json_t *env_value,
        const struct json_t *security, int readable);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_DEFAULTPLU_F7FEC3_H */
