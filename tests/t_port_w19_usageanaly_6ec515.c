#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_usageanaly_6ec515.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static int bool_field(json_t *in, const char *key, int def) {
    json_t *v = json_obj_get(in, key);
    if (!v || v->type != JSON_BOOL) return def;
    return v->bool_val ? 1 : 0;
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
        if (!strcmp(func, "scoped")) {
            out = w19_usageanaly_6ec515_scoped(json_get_str(in, "before", ""),
                                               json_get_str(in, "after", ""),
                                               json_get_str(in, "alias", "s."));
        } else if (!strcmp(func, "local_surface")) {
            char *s = w19_usageanaly_6ec515_local_surface(json_get_str(in, "alias", ""));
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "query_sql")) {
            out = w19_usageanaly_6ec515_query_sql(json_get_str(in, "base", ""),
                                                  bool_field(in, "has_index", 1),
                                                  bool_field(in, "local_surfaces", 1));
        } else if (!strcmp(func, "query_params")) {
            out = w19_usageanaly_6ec515_query_params(json_obj_get(in, "cutoff"),
                                                     json_obj_get(in, "source"),
                                                     bool_field(in, "local_surfaces", 1));
        } else if (!strcmp(func, "insight_wait")) {
            json_t *busy = json_obj_get(in, "read_busy_timeout_s");
            double b = (busy && busy->type == JSON_NUMBER) ? busy->num_val : 0.0;
            out = w19_usageanaly_6ec515_insight_wait(bool_field(in, "read_only", 0), b);
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
