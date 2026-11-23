/*
 * rfb_filter.c — real C port of upstream Hermes tools/bot_desktop/rfb_filter.py constant tables.
 * PoP: cn_0eaab1f03b_constants @ tools/bot_desktop/rfb_filter.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_0eaab1f03b.h"

json_t *cn_0eaab1f03b_constants(void) {
    json_t *out = json_object();
    json_set(out, "_INPUT_TYPES", ({ json_t *__a = json_array();
        json_append(__a, json_number(251));
        json_append(__a, json_number(255));
        json_append(__a, json_number(4));
        json_append(__a, json_number(5));
        json_append(__a, json_number(6));
      __a; }));
    json_set(out, "_FIXED", ({ json_t *__o = json_object();
        json_set(__o, "0", json_number(20));
        json_set(__o, "150", json_number(10));
        json_set(__o, "255", json_number(12));
        json_set(__o, "3", json_number(10));
        json_set(__o, "4", json_number(8));
        json_set(__o, "5", json_number(6));
      __o; }));
    json_set(out, "_SET_ENCODINGS", json_number(2));
    json_set(out, "_CLIENT_CUT_TEXT", json_number(6));
    json_set(out, "_FENCE", json_number(248));
    json_set(out, "_SET_DESKTOP_SIZE", json_number(251));
    return out;
}
