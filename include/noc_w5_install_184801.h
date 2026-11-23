#ifndef NOC_W5_INSTALL_184801_H
#define NOC_W5_INSTALL_184801_H
#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

/* C home for upstream Hermes pm/install.py (gate w5_install_184801).
 * The pure naming / identity / ordering helpers are ported; the install
 * transaction itself (locks, store, network, facts) stays in Python. */

/* PoP: w5_install_184801_extra_key @ pm/install.py:_extra_key */
char *w5_install_184801_extra_key(const char *name);

/* PoP: w5_install_184801_identity @ pm/install.py:_identity
 * Returns JSON null, or [target, [sha256, ...]]. */
json_t *w5_install_184801_identity(const char *target,
                                   const char *const *sha256s, size_t n);

/* PoP: w5_install_184801_dependents_first @ pm/install.py:_dependents_first
 * deps[i] is the NULL-terminated dependency-name list for names[i].
 * Returns a malloc'd NULL-terminated array of names (caller frees via
 * w5_install_184801_strings_free). */
char **w5_install_184801_dependents_first(const char *const *names, size_t n_names,
                                          const char *const *const *deps,
                                          size_t *out_n);

/* PoP: w5_install_184801_strings_free @ pm/install.py:<module> */
void w5_install_184801_strings_free(char **arr, size_t n);

/* PoP: w5_install_184801_free @ pm/install.py:<module> */
void w5_install_184801_free(char *s);

#endif /* NOC_W5_INSTALL_184801_H */
