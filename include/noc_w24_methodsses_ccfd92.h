/*
 * noc_w24_methodsses_ccfd92.h - C surface for upstream
 * tui_gateway/methods_session_foreign.py (gate w24_methodsses_ccfd92).
 *
 * The foreign-history RPC handlers' value layer: the JSON-RPC response
 * envelope per outcome, the params defaults (offset 0, limit 25), the code each
 * failure maps to (-32602 for ValueError, -32000 for OSError / no store) and
 * the two fixed OSError texts.  The browser reader, the profile store and the
 * persistence handle are seams: the behaviour object says what they answer.
 */
#ifndef W24_METHODSSES_CCFD92_H
#define W24_METHODSSES_CCFD92_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req: {"rid": any, "params": obj,
 *       "behavior": {"result": any, "raise": ""|"ValueError"|"OSError", "message": str}}
 * -> {"call": [source, offset, limit], "response": <jsonrpc envelope>} */
json_t *w24_methodsses_ccfd92_dispatch_list(const json_t *req);

/* req: {"rid": any, "params": obj, "importing": bool,
 *       "behavior": {"db": bool, "profile_name": str, "result": any,
 *                    "raise": ""|"ValueError"|"OSError", "message": str}}
 * -> {"call": [fn, id, profile_name] | null, "response": <jsonrpc envelope>} */
json_t *w24_methodsses_ccfd92_dispatch_history(const json_t *req);

/* The registered method names, sorted. */
json_t *w24_methodsses_ccfd92_methods(void);

/* Method names, error codes and the fixed OSError texts. */
json_t *w24_methodsses_ccfd92_constants(void);

#ifdef __cplusplus
}
#endif
#endif
