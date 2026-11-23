#ifndef NOC_W9_TOOLCALLBA_449062_H
#define NOC_W9_TOOLCALLBA_449062_H

#include "hermes_json.h"

/* agent/tool_call_batches.py — native provider block splitting. */
/* Mutates a copy of provider_data and returns it; replacements maps tool id ->
 * array of children, each {"id", "function": {"name", "arguments"}}. */
json_t *w9_toolcallba_449062_replace_native_tool_blocks(json_t *provider_data, json_t *replacements);

#endif /* NOC_W9_TOOLCALLBA_449062_H */
