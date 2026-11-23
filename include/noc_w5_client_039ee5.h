#ifndef NOC_W5_CLIENT_039EE5_H
#define NOC_W5_CLIENT_039EE5_H
#include "hermes_json.h"

/* C home for upstream Hermes pm/client.py (gate w5_client_039ee5).
 * The worker dispatch is IO/process bound and is injected by the gate; what
 * is ported here is the pure part of each public operation: its sequence
 * guard (Python raises TypeError for a str where a sequence is required) and
 * the exact request payload it hands to _python_operation. Each function
 * returns {"op": <name>, "payload": <object>}, or null on the guard. */

/* PoP: w5_client_039ee5_prepare_tools @ pm/client.py:prepare_tools */
json_t *w5_client_039ee5_prepare_tools(json_t *names, const char *out,
                                       const char *target, json_t *cache);

/* PoP: w5_client_039ee5_stage_tools @ pm/client.py:stage_tools */
json_t *w5_client_039ee5_stage_tools(json_t *names, const char *source_store,
                                     const char *out, const char *target);

/* PoP: w5_client_039ee5_ensure_environment @ pm/client.py:ensure_environment */
json_t *w5_client_039ee5_ensure_environment(const char *name, json_t *requirements,
                                            json_t *root, int explicit,
                                            double timeout);

/* PoP: w5_client_039ee5_ensure_python_tool @ pm/client.py:ensure_python_tool */
json_t *w5_client_039ee5_ensure_python_tool(const char *name, json_t *requirements,
                                            const char *executable, json_t *root,
                                            int explicit, double timeout);

/* PoP: w5_client_039ee5_build_requirements_environment
 *   @ pm/client.py:build_requirements_environment */
json_t *w5_client_039ee5_build_requirements_environment(
    json_t *requirements, const char *out, json_t *python, json_t *cache,
    json_t *env, json_t *wheelhouse, int offline, int sealed, int explicit,
    json_t *lock);

#endif /* NOC_W5_CLIENT_039EE5_H */
