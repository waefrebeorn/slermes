/*
 * delegate_pure.h — minimal declaration surface for the deterministic,
 * network/IO-free delegate_tool.py helpers ported in src/tools/delegate.c.
 *
 * Opaque / minimal: no god-header. These three helpers are pure string/struct
 * transforms (tool-content stringification, conservative error detection,
 * role normalization) and are verified by tests/t_port_delegate_pure.c against
 * the live Python oracle.
 */

#ifndef HERMES_DELEGATE_PURE_H
#define HERMES_DELEGATE_PURE_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"   /* json_node_t */

/* Stable text representation for tool-result content (string/list/dict). */
char *delegate_stringify_tool_content(const json_node_t *content, char *out, size_t out_sz);

/* Conservative error-output detector for tool-result previews. */
bool delegate_looks_like_error_output(const json_node_t *content);

/* Normalize a delegate role to "leaf" or "orchestrator" (unknown -> "leaf"). */
void delegate_normalize_role(const char *r, char *out, size_t out_sz);

#endif /* HERMES_DELEGATE_PURE_H */
