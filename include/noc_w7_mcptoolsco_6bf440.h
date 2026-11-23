/* Public C API for the tools/mcp_tool_scope.py parity port. */
#ifndef NOC_W7_MCPTOOLSCO_6BF440_H
#define NOC_W7_MCPTOOLSCO_6BF440_H
#include "hermes_json.h"

/* A ServerKey is encoded as a JSON string (bare name) or a 2-item
 * JSON array [scope, name] (scoped key). */
json_t *w7_mcptoolsco_6bf440_key_name(json_t *key);
json_t *w7_mcptoolsco_6bf440_key_scope(json_t *key);
json_t *w7_mcptoolsco_6bf440_key_visible_in_scope(json_t *key, const char *scope, json_t *adopted);
json_t *w7_mcptoolsco_6bf440_server_key(const char *name, json_t *scope, int current, json_t *current_scope);
json_t *w7_mcptoolsco_6bf440_resolve_server_key(const char *name, json_t *scope, int current,
        json_t *current_scope, json_t *servers, json_t *lazy_configs, json_t *shared);
#endif /* NOC_W7_MCPTOOLSCO_6BF440_H */
