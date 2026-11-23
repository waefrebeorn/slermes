#ifndef NOC_W15_PLUGINHOST_60B0EA_H
#define NOC_W15_PLUGINHOST_60B0EA_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* C11 port of the value/error wire helpers in hermes_cli/plugin_host_wire.py
 * (gate w15_pluginhost_60b0ea). Returned json_t trees are caller-owned. */

/* PoP: w15_pluginhost_60b0ea_error_payload @ hermes_cli/plugin_host_wire.py:error_payload
 * {"type":<class>, "message":<str>, "base":<nearest remote base, if different>}. */
struct json_t *w15_pluginhost_60b0ea_error_payload(const char *type_name,
                                                  const char *message);

/* PoP: w15_pluginhost_60b0ea_raise_remote @ hermes_cli/plugin_host_wire.py:raise_remote
 * {"cls":<raised class name>, "message":<str(exc)>}. */
struct json_t *w15_pluginhost_60b0ea_raise_remote(const char *type_name,
                                                 const char *message,
                                                 const char *base);

/* PoP: w15_pluginhost_60b0ea_decode @ hermes_cli/plugin_host_wire.py:decode
 * Inverse of encode. __record__ -> its decoded fields dict; __opaque__ ->
 * "<opaque TYPE>"; lists/dicts/scalars pass through. `value` is borrowed. */
struct json_t *w15_pluginhost_60b0ea_decode(struct json_t *value);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_PLUGINHOST_60B0EA_H */
