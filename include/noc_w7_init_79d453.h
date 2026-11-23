#ifndef NOC_W7_INIT_79D453_H
#define NOC_W7_INIT_79D453_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Observable surface of the trivial Hermes acp_adapter/__init__.py shim.
 * The upstream module is a single docstring; it binds no non-dunder names. */

/* PoP: w7_init_79d453_doc @ acp_adapter/__init__.py:__doc__ */
const char *w7_init_79d453_doc(void);

/* PoP: w7_init_79d453_public_names @ acp_adapter/__init__.py:<module> */
const char *const *w7_init_79d453_public_names(size_t *count);

/* PoP: w7_init_79d453_has_symbol @ acp_adapter/__init__.py:<module> */
int w7_init_79d453_has_symbol(const char *name);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W7_INIT_79D453_H */
