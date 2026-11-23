#ifndef NOC_W13_BROWSERPAG_472FC0_H
#define NOC_W13_BROWSERPAG_472FC0_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* C11 port of the interface contract declared by agent/browser_page_backend.py
 * (gate w13_browserpag_472fc0). That upstream module is an abstract base class
 * with no computational body; its entire observable content is the method
 * contract below. Each return mirrors what Python introspection reports on the
 * live BrowserPageBackend class. Returned json_t trees are caller-owned. */

/* sorted(BrowserPageBackend.__abstractmethods__) -> ["evaluate","has_page"]. */
struct json_t *w13_browserpag_472fc0_abstract_methods(void);

/* inspect.isabstract(BrowserPageBackend) -> 1/0. */
int w13_browserpag_472fc0_is_abstract(void);

/* Signature shape of one method (name without "self"):
 *   {"params":[...], "kwonly":[...], "abstract":bool}
 * Returns json_null() for a method the class does not declare. */
struct json_t *w13_browserpag_472fc0_method_shape(const char *method);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W13_BROWSERPAG_472FC0_H */
