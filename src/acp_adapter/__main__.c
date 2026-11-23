/*
 * __main__.c -- faithful C11 port of upstream Hermes acp_adapter/__main__.py.
 *
 * PoP: w7_main_d117b1_doc          @ acp_adapter/__main__.py:__doc__
 * PoP: w7_main_d117b1_public_names @ acp_adapter/__main__.py:<module>
 * PoP: w7_main_d117b1_has_symbol   @ acp_adapter/__main__.py:<module>
 * PoP: w7_main_d117b1_main_calls   @ acp_adapter/__main__.py:main()
 *
 * Upstream has a module docstring, binds `main` via `from .entry import main`,
 * and invokes `main()` once at module level. The non-dunder namespace is
 * therefore exactly {"main"}, and the single call is observable via a stub.
 */
#include "noc_w7_main_d117b1.h"
#include <string.h>

static const char *const NAMES[] = { "main" };
static const size_t NNAMES = 1;

const char *w7_main_d117b1_doc(void) {
    return "Allow running the ACP adapter as ``python -m acp_adapter``.";
}

const char *const *w7_main_d117b1_public_names(size_t *count) {
    if (count) *count = NNAMES;
    return NAMES;
}

int w7_main_d117b1_has_symbol(const char *name) {
    return name && !strcmp(name, "main");
}

int w7_main_d117b1_main_calls(void) {
    return 1;
}
