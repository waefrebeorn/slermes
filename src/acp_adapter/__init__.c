/*
 * __init__.c -- faithful C11 port of upstream Hermes acp_adapter/__init__.py.
 *
 * PoP: w7_init_79d453_doc          @ acp_adapter/__init__.py:__doc__
 * PoP: w7_init_79d453_public_names @ acp_adapter/__init__.py:<module>
 * PoP: w7_init_79d453_has_symbol   @ acp_adapter/__init__.py:<module>
 *
 * The upstream module contains only a docstring and no imports, so after
 * import it binds no non-dunder names. Its whole observable contract is the
 * docstring plus an empty public namespace.
 */
#include "noc_w7_init_79d453.h"
#include <string.h>

static const char *const NAMES[] = { NULL };
static const size_t NNAMES = 0;

const char *w7_init_79d453_doc(void) {
    return "ACP (Agent Communication Protocol) adapter for hermes-agent.";
}

const char *const *w7_init_79d453_public_names(size_t *count) {
    if (count) *count = NNAMES;
    return NAMES;
}

int w7_init_79d453_has_symbol(const char *name) {
    (void)name;
    return 0;
}
