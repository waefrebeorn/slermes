#ifndef NOC_W9_PLUGINOAUTH_2F3A26_H
#define NOC_W9_PLUGINOAUTH_2F3A26_H

#ifdef __cplusplus
extern "C" {
#endif

struct json_t;

/* PoP: w9_pluginoauth_2f3a26_valid_id @ hermes_cli/plugin_oauth.py:_ID_RE (^[a-z0-9][a-z0-9-]*$) */
int w9_pluginoauth_2f3a26_valid_id(const char *value);
/* PoP: w9_pluginoauth_2f3a26_valid_env @ hermes_cli/plugin_oauth.py:_ENV_RE (^[A-Z][A-Z0-9_]*$) */
int w9_pluginoauth_2f3a26_valid_env(const char *value);
/* PoP: w9_pluginoauth_2f3a26_valid_host @ hermes_cli/plugin_oauth.py:_HOST_RE */
int w9_pluginoauth_2f3a26_valid_host(const char *value);
/* PoP: w9_pluginoauth_2f3a26_parent @ hermes_cli/plugin_oauth.py:_parent */
struct json_t *w9_pluginoauth_2f3a26_parent(const char *host);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W9_PLUGINOAUTH_2F3A26_H */
