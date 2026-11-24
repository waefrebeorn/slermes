#ifndef NOC_W18_SESSIONROU_7912BD_H
#define NOC_W18_SESSIONROU_7912BD_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w18_sessionrou_7912bd_messaging_platform @ gateway/session_route_recovery.py:_messaging_platform
 * Return the messaging Platform label when `label` names one (local / api_server
 * and unknown labels return NULL), mirroring Platform(label) with a fixed enum. */
const char *w18_sessionrou_7912bd_messaging_platform(const char *label);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W18_SESSIONROU_7912BD_H */
