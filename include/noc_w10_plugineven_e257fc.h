#ifndef NOC_W10_PLUGINEVEN_E257FC_H
#define NOC_W10_PLUGINEVEN_E257FC_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w10_plugineven_e257fc_event_name @ hermes_cli/plugin_events.py:plugin_event_name
 * Returns {"ok":true,"value":"plugin.<id>.<event>"} or
 * {"ok":false,"error":"ValueError"} when the id/event cannot form a wire name.
 * Caller frees. */
json_t *w10_plugineven_e257fc_event_name(const char *plugin_id, const char *event);

/* PoP: w10_plugineven_e257fc_session_event_body @ hermes_cli/plugin_events.py:emit_plugin_session_event
 * Faithful pure prelude of emit_plugin_session_event: validates session_id,
 * validates payload (dict or None), composes the wire name and the JSON-safe
 * body. Returns {"ok":true,"name":...,"body":...} or
 * {"ok":false,"error":"ValueError"|"TypeError"}. Caller frees. */
json_t *w10_plugineven_e257fc_session_event_body(const char *session_id,
                                                 const char *plugin_id,
                                                 const char *event,
                                                 const json_t *payload);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W10_PLUGINEVEN_E257FC_H */
