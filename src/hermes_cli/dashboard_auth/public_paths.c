/*
 * public_paths.c — real C port of upstream Hermes hermes_cli/dashboard_auth/public_paths.py constant tables.
 * PoP: cn_b0510ae425_constants @ hermes_cli/dashboard_auth/public_paths.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_b0510ae425.h"

json_t *cn_b0510ae425_constants(void) {
    json_t *out = json_object();
    json_set(out, "PUBLIC_API_PATHS", ({ json_t *__a = json_array();
        json_append(__a, json_string("/api/config/defaults"));
        json_append(__a, json_string("/api/config/schema"));
        json_append(__a, json_string("/api/cron/fire"));
        json_append(__a, json_string("/api/dashboard/plugins"));
        json_append(__a, json_string("/api/dashboard/themes"));
        json_append(__a, json_string("/api/health"));
        json_append(__a, json_string("/api/model/info"));
        json_append(__a, json_string("/api/status"));
      __a; }));
    return out;
}
