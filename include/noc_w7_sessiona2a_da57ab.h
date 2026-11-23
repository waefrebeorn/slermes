#ifndef NOC_W7_SESSIONA2A_DA57AB_H
#define NOC_W7_SESSIONA2A_DA57AB_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w7_sessiona2a_da57ab_is_forward_policy @ gateway/session_a2a.py:is_forward_policy */
int w7_sessiona2a_da57ab_is_forward_policy(const char *source, const char *platform,
        const char *request_json);

/* PoP: w7_sessiona2a_da57ab_forward_author @ gateway/session_a2a.py:forward_author
 * Returns the authenticated-peer author dict, or JSON null when not a forward policy. */
struct json_t *w7_sessiona2a_da57ab_forward_author(const char *source, const char *platform,
        const char *request_json);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W7_SESSIONA2A_DA57AB_H */
