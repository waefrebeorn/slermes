#ifndef NOC_W12_SESSIONDIS_3B3FBE_H
#define NOC_W12_SESSIONDIS_3B3FBE_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w12_sessiondis_3b3fbe_discover @ gateway/session_discovery.py:discover_commands
 * Full validation outcome: permission_denied / invalid_params / profile_mismatch / ok.
 * profile_ok is the profile_matches_home seam injected from the fixture. */
struct json_t *w12_sessiondis_3b3fbe_discover(const struct json_t *capabilities,
                                               const struct json_t *actor_profile,
                                               const struct json_t *authority_profile,
                                               const struct json_t *params,
                                               const struct json_t *completion,
                                               const struct json_t *profile_ok);
/* PoP: w12_sessiondis_3b3fbe_list_check @ gateway/session_discovery.py:list_plugins
 * Validation outcome: permission_denied / invalid_params / ok. */
struct json_t *w12_sessiondis_3b3fbe_list_check(const struct json_t *capabilities,
                                                 const struct json_t *actor_profile,
                                                 const struct json_t *authority_profile,
                                                 const struct json_t *params);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W12_SESSIONDIS_3B3FBE_H */
