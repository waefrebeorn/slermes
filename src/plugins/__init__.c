/*
 * plugins/__init__.c — faithful C11 port of upstream Hermes plugins/__init__.py.
 *
 * PoP: w6_init_d99929_doc          @ plugins/__init__.py:__doc__
 * PoP: w6_init_d99929_public_names @ plugins/__init__.py:<module>
 * PoP: w6_init_d99929_has_symbol   @ plugins/__init__.py:<module>
 *
 * The upstream module is a single comment (`# Hermes plugins package`).
 * Importing it binds no non-dunder names and defines no docstring, so the
 * entire observable contract is an empty namespace and a null docstring.
 */
#include "noc_w6_init_d99929.h"
#include <string.h>

/* No public (non-dunder) names are bound by the shim. */
static const char *const NAMES[] = { NULL };
static const size_t NNAMES = 0;

const char *w6_init_d99929_doc(void) {
    return NULL; /* upstream has no module docstring */
}

const char *const *w6_init_d99929_public_names(size_t *count) {
    if (count) *count = NNAMES;
    return NAMES;
}

int w6_init_d99929_has_symbol(const char *name) {
    (void)name;
    /* No non-dunder names exist; dunders are excluded by contract anyway. */
    return 0;
}
