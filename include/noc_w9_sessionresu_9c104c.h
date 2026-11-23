#ifndef NOC_W9_SESSIONRESU_9C104C_H
#define NOC_W9_SESSIONRESU_9C104C_H
#include "hermes_json.h"

/* PoP port of gateway/session_results.py — ContextVar-backed failure commit.
 * `captured` is the execution_result context value supplied explicitly so the
 * C port needs no thread-local context: the harness passes it in and receives
 * the mutated value back as part of the exported object. */
json_t *w9_sessionresu_9c104c_record_unexecuted_failure(json_t *reply,
        const char *reason, json_t *error, json_t *captured);
json_t *w9_sessionresu_9c104c_record_terminal_failure(json_t *result,
        json_t *captured);
#endif /* NOC_W9_SESSIONRESU_9C104C_H */
