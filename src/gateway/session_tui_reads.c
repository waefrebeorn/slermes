/*
 * session_tui_reads.c — C home for upstream Hermes gateway/session_tui_reads.py.
 *
 * PoP: w12_sessiontui_e1e835_tui_read @ gateway/session_tui_reads.py:tui_read
 * PoP: w12_sessiontui_e1e835_saved_route @ gateway/session_tui_reads.py:_saved_route
 */
#include "noc_w12_sessiontui_e1e835.h"

#include "hermes_json.h"

#include <string.h>

int w12_sessiontui_e1e835_tui_read(const char *policy_source, const char *name) {
    if (!policy_source || strcmp(policy_source, "tui")) return 0; /* _TUI_SOURCES == {'tui'} */
    if (!name) return 0;
    if (!strcmp(name, "status")) return 1;
    if (!strcmp(name, "context")) return 2;
    return 0;
}

void w12_sessiontui_e1e835_saved_route(const struct json_t *row, struct json_t **out) {
    json_t *result = json_object();
    json_t *cfg = NULL;
    if (row && row->type == JSON_OBJECT) {
        cfg = json_obj_get(row, "model_config");
        json_t *model = json_obj_get(row, "model");
        json_set(result, "model", model ? json_copy(model) : json_null());
    } else {
        json_set(result, "model", json_null());
    }

    json_t *provider = (cfg && cfg->type == JSON_OBJECT) ? json_obj_get(cfg, "provider") : NULL;
    json_t *base_url = (cfg && cfg->type == JSON_OBJECT) ? json_obj_get(cfg, "base_url") : NULL;
    json_set(result, "provider", provider ? json_copy(provider) : json_null());
    json_set(result, "base_url", base_url ? json_copy(base_url) : json_null());

    if (out) *out = result;
    else json_free(result);
}
