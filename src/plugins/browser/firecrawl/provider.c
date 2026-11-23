/*
 * provider.c — real C port of upstream Hermes plugins/browser/firecrawl/provider.py constant tables.
 * PoP: cn_d2cfe3135a_constants @ plugins/browser/firecrawl/provider.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_d2cfe3135a.h"

json_t *cn_d2cfe3135a_constants(void) {
    json_t *out = json_object();
    json_set(out, "_BASE_URL", json_string("https://api.firecrawl.dev"));
    return out;
}
