/*
 * delegate_tool_tuning.c - C home for upstream Hermes tools/delegate_tool_tuning.py.
 * PoP: w14_delegateto_e37eb6_without_service_tier @ tools/delegate_tool_tuning.py:without_service_tier
 *
 * Pure request-override projection. The module's other helpers resolve live
 * provider/model tiers through hermes_cli.models and agent.codex_service_tiers;
 * only the tier-stripping projection is environment-independent and is ported
 * here (the heavy tier resolution stays in the live Python owner).
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w14_delegateto_e37eb6.h"

static int is_tier_key(const char *k) {
    return !strcmp(k, "service_tier") || !strcmp(k, "speed");
}
static int is_dict(const json_t *v) { return v && v->type == JSON_OBJECT; }

struct json_t *w14_delegateto_e37eb6_without_service_tier(const struct json_t *overrides) {
    json_t *cleaned = json_object();
    if (is_dict(overrides)) {
        for (size_t i = 0; i < json_object_size(overrides); i++) {
            const char *k = json_object_get_key_at(overrides, i);
            if (!k || is_tier_key(k)) continue;
            json_set(cleaned, k, json_copy(json_object_get_at(overrides, i)));
        }
    }
    json_t *extra = json_obj_get(cleaned, "extra_body");
    if (is_dict(extra)) {
        json_t *kept = json_object();
        for (size_t i = 0; i < json_object_size(extra); i++) {
            const char *k = json_object_get_key_at(extra, i);
            if (!k || is_tier_key(k)) continue;
            json_set(kept, k, json_copy(json_object_get_at(extra, i)));
        }
        if (json_object_size(kept)) json_set(cleaned, "extra_body", kept);
        else { json_free(kept); json_obj_del(cleaned, "extra_body"); }
    }
    return cleaned;
}

