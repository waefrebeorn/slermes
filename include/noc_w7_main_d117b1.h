#ifndef NOC_W7_MAIN_D117B1_H
#define NOC_W7_MAIN_D117B1_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Observable surface of Hermes acp_adapter/__main__.py. Upstream is a
 * docstring plus `from .entry import main` and a module-level `main()` call. */

/* PoP: w7_main_d117b1_doc @ acp_adapter/__main__.py:__doc__ */
const char *w7_main_d117b1_doc(void);

/* PoP: w7_main_d117b1_public_names @ acp_adapter/__main__.py:<module> */
const char *const *w7_main_d117b1_public_names(size_t *count);

/* PoP: w7_main_d117b1_has_symbol @ acp_adapter/__main__.py:<module> */
int w7_main_d117b1_has_symbol(const char *name);

/* PoP: w7_main_d117b1_main_calls @ acp_adapter/__main__.py:main() */
int w7_main_d117b1_main_calls(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W7_MAIN_D117B1_H */
