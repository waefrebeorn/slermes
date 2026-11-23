#ifndef NOC_W9_SESSIONHAND_E737D5_H
#define NOC_W9_SESSIONHAND_E737D5_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w9_sessionhand_e737d5_unsettled @ gateway/session_handoff.py:_unsettled
 *
 * The human HandoffRefusal for a HandoffUnsettled(reason).  Returns
 * {"reason":..., "detail":..., "field":null}, or {"error":"KeyError"} for an
 * unknown reason.  Caller frees. */
struct json_t *w9_sessionhand_e737d5_unsettled(const char *reason);

/* PoP: w9_sessionhand_e737d5_destination @ gateway/session_handoff.py:_destination
 *
 * resolve_handoff_home() raised HandoffTargetError(reason); map the reason to
 * the human refusal via _REFUSALS[reason].format(platform=...).  Returns
 * {"reason":"invalid_params", "detail":..., "field":"platform"}, or
 * {"error":"KeyError"} for an unknown reason.  Caller frees. */
struct json_t *w9_sessionhand_e737d5_destination(const char *reason, const char *platform);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W9_SESSIONHAND_E737D5_H */
