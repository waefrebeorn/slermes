#ifndef NOC_LOCALROUTE_H
#define NOC_LOCALROUTE_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: localroute_resolve_launch_route @ gateway/session_local_route.py:resolve_launch_route
 *
 * Returns a NEWLY allocated json_t object holding the resolved params
 * (a copy of `params`, with model/provider/base_url/api_key possibly updated).
 * The caller owns and must json_free() the result.
 *
 * Both arguments are borrowed; neither is modified or freed.
 */
struct json_t *localroute_resolve_launch_route(const struct json_t *params,
                                               const struct json_t *config);

#ifdef __cplusplus
}
#endif

#endif /* NOC_LOCALROUTE_H */
