/*
 * adapter_choice_cards.c — real C port of upstream Hermes plugins/platforms/feishu/adapter_choice_cards.py constant tables.
 * PoP: cn_1978b51517_constants @ plugins/platforms/feishu/adapter_choice_cards.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_1978b51517.h"

json_t *cn_1978b51517_constants(void) {
    json_t *out = json_object();
    json_set(out, "_BUTTON_LABEL_MAX", json_number(40));
    return out;
}
