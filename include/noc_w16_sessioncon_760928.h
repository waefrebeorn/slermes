#ifndef NOC_W16_SESSIONCON_760928_H
#define NOC_W16_SESSIONCON_760928_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w16_sessioncon_760928_refused_turn_result @ gateway/session_context_refs.py:refused_turn_result */
struct json_t *w16_sessioncon_760928_refused_turn_result(const struct json_t *row,
                                                         const struct json_t *refusal);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_SESSIONCON_760928_H */
