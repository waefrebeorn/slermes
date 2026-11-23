#ifndef NOC_W9_CLOUDAGENTS_E05D38_H
#define NOC_W9_CLOUDAGENTS_E05D38_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w9_cloudagents_e05d38_org @ hermes_cli/cloud_agents.py:_org */
struct json_t *w9_cloudagents_e05d38_org(const struct json_t *raw);

/* PoP: w9_cloudagents_e05d38_agent @ hermes_cli/cloud_agents.py:_agent */
struct json_t *w9_cloudagents_e05d38_agent(const struct json_t *raw);

/* PoP: w9_cloudagents_e05d38_discover_response @ hermes_cli/cloud_agents.py:discover
 * Pure response-shaping half of discover(): the network/auth seam is injected
 * by the fixture (status_code + JSON body + optional org). Errors are returned
 * as {"status":..,"code":..,"message":..} instead of raising. */
struct json_t *w9_cloudagents_e05d38_discover_response(int status_code,
                                                      const struct json_t *body,
                                                      const char *org);

/* PoP: w9_cloudagents_e05d38_portal_signin_status @ hermes_cli/cloud_agents.py:PORTAL_SIGNIN_STATUS */
int w9_cloudagents_e05d38_portal_signin_status(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W9_CLOUDAGENTS_E05D38_H */
