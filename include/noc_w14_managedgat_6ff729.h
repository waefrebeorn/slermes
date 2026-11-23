#ifndef NOC_W14_MANAGEDGAT_6FF729_H
#define NOC_W14_MANAGEDGAT_6FF729_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_managedgat_6ff729_connector_gateway_origin @ tools/managed_gateway_auth.py:connector_gateway_origin */
const char *w14_managedgat_6ff729_connector_gateway_origin(const char *connector_url);

/* PoP: w14_managedgat_6ff729_is_managed @ tools/managed_gateway_auth.py:is_managed_nous_gateway_url */
int w14_managedgat_6ff729_is_managed(const char *url, const char *tool_url,
                                     const char *connector_url);

/* PoP: w14_managedgat_6ff729_auth_headers @ tools/managed_gateway_auth.py:managed_gateway_auth_headers */
struct json_t *w14_managedgat_6ff729_auth_headers(const char *url, const char *tool_url,
                                                  const char *connector_url, const char *token);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_MANAGEDGAT_6FF729_H */
