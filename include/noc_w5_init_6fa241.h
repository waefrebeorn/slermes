#ifndef NOC_W5_INIT_6FA241_H
#define NOC_W5_INIT_6FA241_H
#include <stddef.h>

/* Observable surface of the trivial agent/__init__.py package shim.
 * The module's whole behaviour is a docstring plus `from . import
 * jiter_preload as _jiter_preload`; there are no callable Python functions. */

/* PoP: w5_init_6fa241_doc @ agent/__init__.py:__doc__ */
const char *w5_init_6fa241_doc(void);

/* PoP: w5_init_6fa241_public_names @ agent/__init__.py:<module>
 * Non-dunder names bound by the package shim (dir() minus '__*'). */
const char *const *w5_init_6fa241_public_names(size_t *count);

/* PoP: w5_init_6fa241_has_symbol @ agent/__init__.py:<module>
 * True iff name is a non-dunder attribute bound on the package shim. */
int w5_init_6fa241_has_symbol(const char *name);

#endif /* NOC_W5_INIT_6FA241_H */
