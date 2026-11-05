#ifndef PORT_SIDE_QUESTION_REQUEST_H
#define PORT_SIDE_QUESTION_REQUEST_H

#include "hermes_json.h"

/* Port of Python: agent/side_question_request.py — the pure request-shape
 * checks of the text-only side question.
 *
 * `has_server_side_tools` is the safety gate: it answers whether the FINAL
 * request would ask the PROVIDER to execute something itself, which no
 * client-side drop can stop. The fork/answer/audit path is runtime state.
 */

/* Runtime identity a captured prompt must match before it is sent:
 * [model, provider, base_url, api_mode, reasoning_config]. Missing attributes
 * become null. Returns a JSON array. */
json_t *sq_runtime_key(const char *model, int has_provider, const char *provider,
                       const char *base_url, const char *api_mode,
                       const char *reasoning_config_json);

/* True when a tool entry carries a non-client type, OpenAI web_search_options
 * is present, or an OpenRouter plugin is not routing-only. */
int sq_has_server_side_tools(const char *api_kwargs_json);

#endif /* PORT_SIDE_QUESTION_REQUEST_H */
