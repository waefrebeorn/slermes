#ifndef NOC_W9_MCPOAUTHDE_C3E807_H
#define NOC_W9_MCPOAUTHDE_C3E807_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w9_mcpoauthde_c3e807_positive_seconds @ tools/mcp_oauth_device.py:_positive_seconds */
struct json_t *w9_mcpoauthde_c3e807_positive_seconds(const struct json_t *value, const char *label);
/* PoP: w9_mcpoauthde_c3e807_payload @ tools/mcp_oauth_device.py:_payload */
struct json_t *w9_mcpoauthde_c3e807_payload(int status, const struct json_t *data, int bad_json, const char *label);
/* PoP: w9_mcpoauthde_c3e807_constants @ tools/mcp_oauth_device.py:DEVICE_GRANT */
struct json_t *w9_mcpoauthde_c3e807_constants(void);
/* PoP: w9_mcpoauthde_c3e807_device_grant_supported @ tools/mcp_oauth_device.py:_device_metadata */
int w9_mcpoauthde_c3e807_device_grant_supported(const struct json_t *grants);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W9_MCPOAUTHDE_C3E807_H */
