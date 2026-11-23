#ifndef NOC_W11_MEMORYCALL_6310DC_H
#define NOC_W11_MEMORYCALL_6310DC_H
#include "hermes_json.h"

/* PoP port of agent/memory_caller_scope.py (gate w11_memorycall_6310dc). */
char   *w11_memorycall_6310dc_refused_tool_message(const char *tool_name);
json_t *w11_memorycall_6310dc_bound_scope(json_t *home, json_t *identity, json_t *home_key);
/* caller_home == NULL means "the caller names no profile" (Python None).
 * keys is a JSON object mapping a home string to its resolved key (or null). */
json_t *w11_memorycall_6310dc_caller_scope(json_t *bound, const char *caller_home,
                                           const char *process_home, int multiplexed, json_t *keys);
#endif /* NOC_W11_MEMORYCALL_6310DC_H */
