#ifndef NOC_W8_INIT_8BC62E_H
#define NOC_W8_INIT_8BC62E_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_init_8bc62e_doc @ session_rules/__init__.py:__doc__
 * The package module docstring (the module defines nothing else). */
const char *w8_init_8bc62e_doc(void);

/* PoP: w8_init_8bc62e_public_names @ session_rules/__init__.py:<module>
 * Non-dunder names bound by the package shim: [] for this module. */
struct json_t *w8_init_8bc62e_public_names(void);

/* PoP: w8_init_8bc62e_has_symbol @ session_rules/__init__.py:<module>
 * True iff name is one of the non-dunder bindings (never true here). */
int w8_init_8bc62e_has_symbol(const char *name);

/* PoP: w8_init_8bc62e_is_package @ session_rules/__init__.py:<module>
 * The module is a package (defines __path__). */
int w8_init_8bc62e_is_package(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W8_INIT_8BC62E_H */
