#ifndef NOC_CANON_H
#define NOC_CANON_H
#include "hermes_json.h"
/* Both return malloc'd canonical JSON strings; caller frees. */
/* PoP: canon_canonical_dumps @ persistence/core/canonical.py:canonical_dumps */
char *canon_canonical_dumps(const json_t *value);
/* PoP: canon_canonical_json @ persistence/core/canonical.py:canonical_json */
/* Non-object input sets *refused=1 and returns NULL (AuthorityRefusal). */
char *canon_canonical_json(const json_t *value, int *refused);
#endif
