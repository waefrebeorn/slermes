#ifndef PORT_GATEWAY_WAKE_H
#define PORT_GATEWAY_WAKE_H

#include "hermes_json.h"

/* Port of Python: gateway/wake.py — the adapter-shape predicates and the
 * delegation display metadata.
 *
 * The delivery path itself (adapter admission, FIFO commit, drain gates) is
 * runtime state and is not ported.
 */

/* bool(getattr(adapter, "supports_async_delivery", True)) — an adapter that
 * declares nothing is push-capable. */
int wake_adapter_supports_push(int attr_present, int attr_value);

/* getattr(getattr(adapter, "gateway_runner", None), "_draining", False) is True */
int wake_runner_draining(int runner_present, int draining_present, int draining);

/* Display-only metadata for a persisted delegation delivery row.
 * `warnings_enabled` is the caller's warning_notifications_enabled("api_server")
 * result (config state, so it is an input). Returns a JSON object. */
json_t *wake_delegation_display_metadata(const char *evt_json, int warnings_enabled);

#endif /* PORT_GATEWAY_WAKE_H */
