#ifndef NOC_W7_INIT_51A22B_H
#define NOC_W7_INIT_51A22B_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w7_init_51a22b_doc @ hermes_cli/__init__.py:__doc__ */
const char *w7_init_51a22b_doc(void);

/* PoP: w7_init_51a22b_release_date @ hermes_cli/__init__.py:__release_date__ */
const char *w7_init_51a22b_release_date(void);

/* PoP: w7_init_51a22b_getattr @ hermes_cli/__init__.py:__getattr__
 * Only '__version__' resolves (to str(baseVersion or '0.0.0')); every other
 * name raises AttributeError, represented here as JSON null.
 * base_version is the injected install-stamp value (fixture-driven seam). */
struct json_t *w7_init_51a22b_getattr(const char *name, const struct json_t *base_version);

/* PoP: w7_init_51a22b_public_names @ hermes_cli/__init__.py:<module>
 * Non-dunder names bound on the package shim (dir() minus '__*'). */
struct json_t *w7_init_51a22b_public_names(void);

/* PoP: w7_init_51a22b_has_symbol @ hermes_cli/__init__.py:<module>
 * True iff name is one of those non-dunder bindings. */
int w7_init_51a22b_has_symbol(const char *name);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W7_INIT_51A22B_H */
