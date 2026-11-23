/* t_port_w6_usagegauges_1dd126.c — JSON-lines harness for agent/usage_gauges.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_usagegauges_1dd126.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;

        if (!strcmp(func, "paused_for_payment")) {
            const json_t *r = json_obj_get(in, "reason");
            const char *reason = (r && r->type == JSON_STRING) ? r->str_val : NULL;
            out = json_bool(w6_usagegauges_1dd126_paused_for_payment(reason) != 0);
        } else if (!strcmp(func, "parse_gauges")) {
            out = w6_usagegauges_1dd126_parse_gauges(json_obj_get(in, "value"));
        } else if (!strcmp(func, "gauge_title") || !strcmp(func, "gauge_limit_name") ||
                   !strcmp(func, "gauge_short")) {
            const char *id = json_get_str(in, "id", "");
            const char *label = json_get_str(in, "label", "");
            char *r = !strcmp(func, "gauge_title") ? w6_usagegauges_1dd126_gauge_title(id, label)
                    : !strcmp(func, "gauge_limit_name") ? w6_usagegauges_1dd126_gauge_limit_name(id, label)
                    : w6_usagegauges_1dd126_gauge_short(id, label);
            out = r ? json_string(r) : json_string("");
            free(r);
        } else if (!strcmp(func, "all_scope") || !strcmp(func, "top_tier_gauge") ||
                   !strcmp(func, "binding_gauge")) {
            json_t *pg = w6_usagegauges_1dd126_parse_gauges(json_obj_get(in, "value"));
            if (!strcmp(func, "all_scope")) out = w6_usagegauges_1dd126_all_scope(pg);
            else if (!strcmp(func, "top_tier_gauge")) out = w6_usagegauges_1dd126_top_tier_gauge(pg);
            else out = w6_usagegauges_1dd126_binding_gauge(pg);
            json_free(pg);
        } else {
            out = json_string("UNKNOWN_FUNC");
        }

        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
