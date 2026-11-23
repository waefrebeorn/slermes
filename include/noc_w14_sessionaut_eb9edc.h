#ifndef NOC_W14_SESSIONAUT_EB9EDC_H
#define NOC_W14_SESSIONAUT_EB9EDC_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w14_sessionaut_eb9edc_registry @ gateway/session_authorities.py:SessionAuthorities
 * Stateless replay of a SessionAuthorities registry: processes add/replace/remove
 * ops then answers queries (len, contains, for_home, launch, multiplexed,
 * starting_homes, serving, served_profiles, profile_ids, profile_name, iter).
 * Keys are home strings; an authority is {"id":<profile_id>,"retiring":bool} or null. */
struct json_t *w14_sessionaut_eb9edc_registry(const struct json_t *state);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_SESSIONAUT_EB9EDC_H */
