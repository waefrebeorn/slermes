#ifndef NOC_W14_SESSIONGRO_4089BA_H
#define NOC_W14_SESSIONGRO_4089BA_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w14_sessiongro_4089ba_target_methods @ gateway/session_group_peers.py:TARGET_METHODS
 * The operator-only target methods, sorted. */
struct json_t *w14_sessiongro_4089ba_target_methods(void);
/* PoP: w14_sessiongro_4089ba_room_link @ gateway/session_group_peers.py:room_link
 * state: profile, adapter, durable, secret, catalog_raises, catalog. Returns the
 * enabled/disabled decision, or the enabled payload. */
struct json_t *w14_sessiongro_4089ba_room_link(const struct json_t *state);
/* PoP: w14_sessiongro_4089ba_invite @ gateway/session_group_peers.py:_invite
 * state: params (authority_epoch is a decimal STRING), link_enabled, profile,
 * invitation. Validates params then shapes the invitation, or an error reason. */
struct json_t *w14_sessiongro_4089ba_invite(const struct json_t *state);
/* PoP: w14_sessiongro_4089ba_revoke @ gateway/session_group_peers.py:_revoke
 * state: grant, decode_raises, claims, profile, gateway_id. Validates the grant
 * and its target, then returns revoked=true or an error reason. */
struct json_t *w14_sessiongro_4089ba_revoke(const struct json_t *state);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_SESSIONGRO_4089BA_H */
