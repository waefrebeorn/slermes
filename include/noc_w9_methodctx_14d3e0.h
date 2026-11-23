#ifndef NOC_W9_METHODCTX_14D3E0_H
#define NOC_W9_METHODCTX_14D3E0_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Pure registrar / publish-skip semantics of tui_gateway/method_ctx.py.
 * Every symbol is prefixed with the gate id. */

/* PoP: w9_methodctx_14d3e0_names @ tui_gateway/method_ctx.py:HandlerRegistry.names
 * methods: JSON array of method-name strings (as passed to HandlerRegistry.method).
 * Returns sorted(set(names)) as a JSON array of strings. Caller frees. */
json_t *w9_methodctx_14d3e0_names(const json_t *methods);

/* PoP: w9_methodctx_14d3e0_skip @ tui_gateway/method_ctx.py:bind_module
 * Mirrors the publish skip test: name.startswith("__") or name in _PLUMBING
 * or name in skip (list of strings). */
int w9_methodctx_14d3e0_skip(const char *name, const json_t *skip);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W9_METHODCTX_14D3E0_H */
