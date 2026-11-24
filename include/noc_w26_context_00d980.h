/*
 * noc_w26_context_00d980.h - C surface for upstream
 * persistence/sqlite/workers/_context.py (gate w26_context_00d980).
 *
 * The closed constructor-context operations of the worker receipt transaction.
 * Every handler is a payload gate plus one SQL statement: the statement, the
 * connection and the holder are seams, so what is ported here is exactly the
 * gate -- which payload shapes the owner admits for each operation, and the
 * prompt-cache clock stamp that rewrites a JSON event with the owner's durable
 * context generation.
 */
#ifndef NOC_W26_CONTEXT_00D980_H
#define NOC_W26_CONTEXT_00D980_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Largest encoded tool-names pin the receipt accepts (RECEIPT_PAYLOAD_MAX_BYTES). */
#define W26_CONTEXT_00D980_PAYLOAD_MAX_BYTES (4u * 1024u * 1024u)
/* Longest single tool name / delegation backend a pin may carry. */
#define W26_CONTEXT_00D980_MAX_TOOL_NAME 256
#define W26_CONTEXT_00D980_MAX_BACKEND 64

/* req: {"op": <one of the operation names>, "payload": <any>} ->
 * null when the owner admits the payload, else the refusal code string
 * ("invalid_params"; "permission_denied" is a conn seam and never returned). */
json_t *w26_context_00d980_validate(const json_t *req);

/* req: {"clock": <str>, "generation": <number>} -> {"parsed": bool, "value": ...}
 * parsed=false carries the clock string back untouched (unparseable or not an
 * object); parsed=true carries the event object with "generation" stamped. */
json_t *w26_context_00d980_stamp(const json_t *req);

/* The operation vocabulary and the payload bounds, as an object. */
json_t *w26_context_00d980_constants(void);

#ifdef __cplusplus
}
#endif
#endif
