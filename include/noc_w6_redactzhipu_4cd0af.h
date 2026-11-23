#ifndef NOC_W6_REDACTZHIPU_4CD0AF_H
#define NOC_W6_REDACTZHIPU_4CD0AF_H

/*
 * Public C API for the port of agent/redact_zhipu.py (gate w6_redactzhipu_4cd0af).
 * The upstream module is a single compiled regex; we port its exact match
 * semantics. All exported symbols are redactzhipu_-prefixed.
 */
#include "hermes_json.h"

/* Port of agent.redact_zhipu._ZHIPU_API_KEY_RE.finditer(text): returns a JSON
 * array of group(1) strings for every non-overlapping match, in order.
 * Pattern: (?<![A-Za-z0-9_.-])([0-9a-f]{32}\.[A-Za-z0-9]{16,})(?![A-Za-z0-9_.-])
 * Caller frees the returned json_t. */
json_t *redactzhipu_find(const char *text);

#endif /* NOC_W6_REDACTZHIPU_4CD0AF_H */
