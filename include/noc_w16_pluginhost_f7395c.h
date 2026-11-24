#ifndef NOC_W16_PLUGINHOST_F7395C_H
#define NOC_W16_PLUGINHOST_F7395C_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w16_pluginhost_f7395c_decode @ hermes_cli/plugin_host_child.py:decode */
struct json_t *w16_pluginhost_f7395c_decode(const struct json_t *value);
/* PoP: w16_pluginhost_f7395c_host_unsupported_reason @ hermes_cli/plugin_host_child.py:_host_unsupported_reason */
struct json_t *w16_pluginhost_f7395c_host_unsupported_reason(const struct json_t *overrides_create_client);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_PLUGINHOST_F7395C_H */
