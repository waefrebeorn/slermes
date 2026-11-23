#ifndef NOC_W10_SESSIONTUI_131199_H
#define NOC_W10_SESSIONTUI_131199_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w10_sessiontui_131199_abandoned @ gateway/session_tui_side.py:_abandoned
 * holder is either null/[] or [ {"abandoned": <bool|null>} ]; the value models a
 * callable's return (absent key or null -> holder[0].abandoned is None). Returns JSON bool. */
struct json_t *w10_sessiontui_131199_abandoned(const struct json_t *holder);
/* PoP: w10_sessiontui_131199_admit_valid @ gateway/session_tui_side.py:_admit
 * The admission validation half of _admit (the AuthorityRefusal('invalid_params') guard).
 * sid mirrors ref.session_id; params/optional are JSON object/array. Returns JSON bool. */
struct json_t *w10_sessiontui_131199_admit_valid(const struct json_t *params,
                                                 const struct json_t *sid,
                                                 const char *required,
                                                 const struct json_t *optional);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W10_SESSIONTUI_131199_H */
