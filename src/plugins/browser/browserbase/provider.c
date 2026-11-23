/*
 * provider.c — real C port of upstream Hermes plugins/browser/browserbase/provider.py constant tables.
 * PoP: cn_eff47defba_constants @ plugins/browser/browserbase/provider.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_eff47defba.h"

json_t *cn_eff47defba_constants(void) {
    json_t *out = json_object();
    json_set(out, "_PAID_FEATURE_FALLBACKS", ({ json_t *__a = json_array();
        json_append(__a, ({ json_t *__a = json_array();
            json_append(__a, json_string("keepAlive"));
            json_append(__a, json_string("keepAlive may require paid plan (402), retrying without it. Sessions may timeout during long operations."));
          __a; }));
        json_append(__a, ({ json_t *__a = json_array();
            json_append(__a, json_string("proxies"));
            json_append(__a, json_string("Proxies unavailable (402), retrying without proxies. Bot detection may be less effective."));
          __a; }));
      __a; }));
    return out;
}
