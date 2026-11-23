/*
 * __init__.c -- faithful C11 port of upstream Hermes hermes_agent/__init__.py.
 *
 * PoP: w8_init_7b876f_doc          @ hermes_agent/__init__.py:__doc__
 * PoP: w8_init_7b876f_public_names @ hermes_agent/__init__.py:<module>
 * PoP: w8_init_7b876f_has_symbol   @ hermes_agent/__init__.py:<module>
 *
 * The upstream module contains only a docstring and no imports, so after
 * import it binds no non-dunder names. Its whole observable contract is the
 * docstring plus an empty public namespace.
 */
#include "noc_w8_init_7b876f.h"
#include <string.h>

static const char *const NAMES[] = { NULL };
static const size_t NNAMES = 0;

const char *w8_init_7b876f_doc(void) {
    return "The Hermes Agent package; today it holds only the module-rename compatibility layer.";
}

const char *const *w8_init_7b876f_public_names(size_t *count) {
    if (count) *count = NNAMES;
    return NAMES;
}

int w8_init_7b876f_has_symbol(const char *name) {
    (void)name;
    return 0;
}
