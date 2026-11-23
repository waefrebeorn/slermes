/*
 * __init__.c — C home for upstream Hermes tui_gateway/__init__.py.
 * PoP: w7_init_ec1211_public_names @ tui_gateway/__init__.py:<module public surface>
 * PoP: w7_init_ec1211_has_attr @ tui_gateway/__init__.py:hasattr(module, name)
 * PoP: w7_init_ec1211_module_doc @ tui_gateway/__init__.py:__doc__
 *
 * The live file is 0 bytes: no imports, no definitions, no docstring. The
 * faithful port therefore exposes only the empty public surface. The dunder
 * names injected by importlib (__name__, __spec__, __builtins__, ...) are
 * excluded by the oracle's non-underscore projection.
 */
#include "noc_w7_init_ec1211.h"

json_t *w7_init_ec1211_public_names(void) {
    return json_array();
}
json_t *w7_init_ec1211_has_attr(const char *name) {
    (void)name;
    return json_bool(false);
}
json_t *w7_init_ec1211_module_doc(void) {
    return json_null();
}
