#ifndef NOC_W5_INIT_7FB779_H
#define NOC_W5_INIT_7FB779_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w5_init_7fb779_all @ cron/__init__.py:__all__
 * The package's declared public surface, in declaration order. */
struct json_t *w5_init_7fb779_all(void);

/* PoP: w5_init_7fb779_exports @ cron/__init__.py:from cron.jobs / cron.scheduler
 * Maps every public name to the module it is re-exported from. */
struct json_t *w5_init_7fb779_exports(void);

/* PoP: w5_init_7fb779_has @ cron/__init__.py:__all__
 * Membership in the package's `from cron import *` surface. */
struct json_t *w5_init_7fb779_has(const char *name);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W5_INIT_7FB779_H */
