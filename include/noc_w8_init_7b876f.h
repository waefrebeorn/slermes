#ifndef NOC_W8_INIT_7B876F_H
#define NOC_W8_INIT_7B876F_H

#include <stddef.h>

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Observable surface of the trivial hermes_agent/__init__.py shim.
 * The upstream module is a single docstring and binds no non-dunder names. */

/* PoP: w8_init_7b876f_doc @ hermes_agent/__init__.py:__doc__ */
const char *w8_init_7b876f_doc(void);

/* PoP: w8_init_7b876f_public_names @ hermes_agent/__init__.py:<module> */
const char *const *w8_init_7b876f_public_names(size_t *count);

/* PoP: w8_init_7b876f_has_symbol @ hermes_agent/__init__.py:<module> */
int w8_init_7b876f_has_symbol(const char *name);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W8_INIT_7B876F_H */
