#ifndef NOC_W15_AUTHDEVICE_1ACD9A_H
#define NOC_W15_AUTHDEVICE_1ACD9A_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_authdevice_1acd9a__names_console_browser @ hermes_cli/auth_device_flow.py:_names_console_browser */
int w15_authdevice_1acd9a_names_console_browser(const char *value);
/* PoP: w15_authdevice_1acd9a__nous_device_auth_timeout_message @ hermes_cli/auth_device_flow.py:_nous_device_auth_timeout_message */
struct json_t *w15_authdevice_1acd9a_nous_device_auth_timeout_message(const char *portal_base_url);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_AUTHDEVICE_1ACD9A_H */
