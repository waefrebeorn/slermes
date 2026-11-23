/*
 * agent/__init__.c — faithful C11 port of upstream Hermes agent/__init__.py.
 *
 * PoP: w5_init_6fa241_doc          @ agent/__init__.py:__doc__
 * PoP: w5_init_6fa241_public_names @ agent/__init__.py:<module>
 * PoP: w5_init_6fa241_has_symbol   @ agent/__init__.py:<module>
 *
 * The upstream module is only a docstring and `from . import jiter_preload as
 * _jiter_preload`. Its entire observable contract is that docstring and the
 * single non-dunder binding; the import side effect (jiter native preload) is
 * process/IO-bound and cannot be captured by a JSON fixture.
 */
#include "noc_w5_init_6fa241.h"
#include <string.h>

static const char *const NAMES[] = { "_jiter_preload" };
static const size_t NNAMES = sizeof(NAMES) / sizeof(NAMES[0]);

const char *w5_init_6fa241_doc(void) {
    return "Agent internals extracted from run_agent.py so it stays focused on AIAgent.";
}

const char *const *w5_init_6fa241_public_names(size_t *count) {
    if (count) *count = NNAMES;
    return NAMES;
}

int w5_init_6fa241_has_symbol(const char *name) {
    if (!name || name[0] == '_' && name[1] == '_') return 0; /* dunders excluded */
    for (size_t i = 0; i < NNAMES; i++)
        if (!strcmp(name, NAMES[i])) return 1;
    return 0;
}
