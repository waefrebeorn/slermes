#ifndef NOC_W18_SESSIONPOL_E8BDA4_H
#define NOC_W18_SESSIONPOL_E8BDA4_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w18_sessionpol_e8bda4_follow @ gateway/session_policy_refresh.py:_follow
 * PoP: w18_sessionpol_e8bda4_server_name @ gateway/session_policy_refresh.py:_server_name
 *
 * ``follow`` returns a copy of ``target`` with ``key`` taken from ``live`` (or
 * removed when absent there). ``server_name`` is an alias resolver:
 * ``mcp-<name>`` maps to ``<name>`` unless ``<name>`` is itself a server.
 * Both return a new value (caller frees).
 */
struct json_t *w18_sessionpol_e8bda4_follow(
    const struct json_t *target, const struct json_t *live, const char *key);
char *w18_sessionpol_e8bda4_server_name(
    const struct json_t *servers, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W18_SESSIONPOL_E8BDA4_H */
