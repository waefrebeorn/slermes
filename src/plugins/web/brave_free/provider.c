/*
 * provider.c — real C port of upstream Hermes plugins/web/brave_free/provider.py constant tables.
 * PoP: cn_62ae795a05_constants @ plugins/web/brave_free/provider.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_62ae795a05.h"

json_t *cn_62ae795a05_constants(void) {
    json_t *out = json_object();
    json_set(out, "_BRAVE_ENDPOINT", json_string("https://api.search.brave.com/res/v1/web/search"));
    return out;
}
