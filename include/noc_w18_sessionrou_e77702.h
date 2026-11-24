#ifndef NOC_W18_SESSIONROU_E77702_H
#define NOC_W18_SESSIONROU_E77702_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w18_sessionrou_e77702_resolved_route @ gateway/session_route_identity.py:resolved_route
 * PoP: w18_sessionrou_e77702_configured_entry_key @ gateway/session_route_identity.py:configured_entry_key
 * PoP: w18_sessionrou_e77702_profile_route_identity @ gateway/session_route_identity.py:profile_route_identity
 *
 * resolved_route returns a 2-element array [provider_identity, base_url];
 * configured_entry_key and profile_route_identity return strings.
 * All return values are newly allocated (caller frees).
 */
struct json_t *w18_sessionrou_e77702_resolved_route(
    const struct json_t *model_cfg, const struct json_t *config);
struct json_t *w18_sessionrou_e77702_configured_entry_key(
    const char *identity, const struct json_t *config);
struct json_t *w18_sessionrou_e77702_profile_route_identity(
    const struct json_t *config);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W18_SESSIONROU_E77702_H */
