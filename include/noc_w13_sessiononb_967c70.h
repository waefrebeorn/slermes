#ifndef NOC_W13_SESSIONONB_967C70_H
#define NOC_W13_SESSIONONB_967C70_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w13_sessiononb_967c70_handlers @ gateway/session_onboarding.py:handlers
 * Sorted keys of the sidecar handler map: onboarding.skip / onboarding.start. */
struct json_t *w13_sessiononb_967c70_handlers(void);

/* PoP: w13_sessiononb_967c70_onboarding_connection @ gateway/session_onboarding.py:_onboarding_connection
 * connection._sibling_for(ONBOARDING_PROFILE) or connection. Sibling is projected
 * as its JSON value; a falsy sibling yields the string "self" (the asking connection). */
struct json_t *w13_sessiononb_967c70_onboarding_connection(const struct json_t *sibling);

/* PoP: w13_sessiononb_967c70_run_id @ gateway/session_onboarding.py:_run_id
 * (load(...) or {}).get('run_id') or '' over the projected load record.
 * A truthy run_id is returned as-is; any falsy/missing one becomes the empty string. */
struct json_t *w13_sessiononb_967c70_run_id(const struct json_t *record);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_SESSIONONB_967C70_H */
