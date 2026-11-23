/*
 * __init__.c — C home for upstream Hermes session_rules/__init__.py.
 * PoP: w8_init_8bc62e_doc @ session_rules/__init__.py:__doc__
 * PoP: w8_init_8bc62e_public_names @ session_rules/__init__.py:<module>
 * PoP: w8_init_8bc62e_has_symbol @ session_rules/__init__.py:<module>
 * PoP: w8_init_8bc62e_is_package @ session_rules/__init__.py:<module>
 */
#include "noc_w8_init_8bc62e.h"
#include "hermes_json.h"
#include <string.h>

const char *w8_init_8bc62e_doc(void) {
    return "Pure session rules the application and every storage backend share "
           "(standard library and the persistence contract's error classes only).";
}

struct json_t *w8_init_8bc62e_public_names(void) {
    /* The live shim binds only dunder names; dir() minus "__*" is empty. */
    return json_array();
}

int w8_init_8bc62e_has_symbol(const char *name) {
    (void)name;   /* no non-dunder bindings exist */
    return 0;
}

int w8_init_8bc62e_is_package(void) {
    return 1;     /* __init__.py defines __path__ */
}
