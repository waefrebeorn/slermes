#ifndef NOC_W5_EXTRAS_3292F9_H
#define NOC_W5_EXTRAS_3292F9_H
#include <stddef.h>

/* C home for upstream Hermes pm/extras.py (gate w5_extras_3292f9).
 * The ANCHORS table and the pure name/anchor helpers are ported; availability
 * probing (find_spec), platform gates and venv installation stay in Python. */

/* PoP: w5_extras_3292f9_canonical @ pm/extras.py:_canonical
 * malloc'd PEP 685 canonical name (runs of -_. collapse to '-', ASCII lower). */
char *w5_extras_3292f9_canonical(const char *name);

/* PoP: w5_extras_3292f9_install_hint @ pm/extras.py:install_hint
 * malloc'd "hermes pm install --extra <extra>". */
char *w5_extras_3292f9_install_hint(const char *extra);

/* PoP: w5_extras_3292f9_anchors @ pm/extras.py:_anchors
 * malloc'd NULL-terminated array of malloc'd anchor strings, *out_n count. */
char **w5_extras_3292f9_anchors(const char *extra, size_t *out_n);

/* PoP: w5_extras_3292f9_missing @ pm/extras.py:missing
 * Anchors not present in the injected importable set, in table order. */
char **w5_extras_3292f9_missing(const char *extra,
                                const char *const *importable, size_t nimp,
                                size_t *out_n);

/* PoP: w5_extras_3292f9_strings_free @ pm/extras.py:<module> */
void w5_extras_3292f9_strings_free(char **arr, size_t n);

#endif /* NOC_W5_EXTRAS_3292F9_H */
