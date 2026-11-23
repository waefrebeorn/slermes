#ifndef NOC_W6_INIT_D99929_H
#define NOC_W6_INIT_D99929_H
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Observable surface of the trivial Hermes plugins/__init__.py shim.
 * The upstream module is a single comment ("# Hermes plugins package"):
 * it binds no public names and declares no docstring.  Its whole observable
 * contract is therefore an empty public namespace and a null docstring. */

/* PoP: w6_init_d99929_doc @ plugins/__init__.py:__doc__
 * Returns NULL (the upstream module has no docstring). */
const char *w6_init_d99929_doc(void);

/* PoP: w6_init_d99929_public_names @ plugins/__init__.py:<module>
 * Non-dunder names bound by the shim (dir() minus '__*') -> empty set. */
const char *const *w6_init_d99929_public_names(size_t *count);

/* PoP: w6_init_d99929_has_symbol @ plugins/__init__.py:<module>
 * True iff name is a non-dunder attribute bound on the package shim. */
int w6_init_d99929_has_symbol(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W6_INIT_D99929_H */
