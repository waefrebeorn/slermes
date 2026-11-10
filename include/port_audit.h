#ifndef SLERMES_PORT_AUDIT_H
#define SLERMES_PORT_AUDIT_H

#include "json.h"

/* _bounded_value(value): recursively bound an audit field — strings longer than
 * 256 get truncated to 241 chars + "...[truncated]"; dict/list recurse. Returns
 * a new json_t tree, caller frees. */
json_t *port_audit_bounded_value(const json_t *value);

#endif /* SLERMES_PORT_AUDIT_H */
