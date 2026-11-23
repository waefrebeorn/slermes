#ifndef NOC_W8_TURNAPIREQ_CC50BE_H
#define NOC_W8_TURNAPIREQ_CC50BE_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_turnapireq_cc50be_set_extra_header @ agent/turn_api_request.py:_set_extra_header
 * Copy-on-write header set: returns a NEW api_kwargs object whose extra_headers
 * dict is a shallow copy with key=value applied. */
struct json_t *w8_turnapireq_cc50be_set_extra_header(const struct json_t *api_kwargs,
                                                     const char *key,
                                                     const struct json_t *value);

/* PoP: w8_turnapireq_cc50be_apply_user_turn_request_headers @ agent/turn_api_request.py:_apply_user_turn_request_headers
 * Returns {"api_kwargs": <updated>, "_is_user_initiated_turn": <post flag>}.
 * request_headers is the injected route_answer("request_headers") result. */
struct json_t *w8_turnapireq_cc50be_apply_user_turn_request_headers(
    const struct json_t *agent, const struct json_t *api_kwargs,
    const struct json_t *request_headers);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W8_TURNAPIREQ_CC50BE_H */
