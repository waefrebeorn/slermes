/* JSON-lines harness for the w34_hostedwork_56ec19 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w34_hostedwork_56ec19.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *dispatch(const char *func, const json_t *in) {
    if (!strcmp(func, "retained_fields")) {
        return w34_hostedwork_56ec19_retained_fields(json_get_str(in, "table", ""));
    }
    if (!strcmp(func, "row_size")) {
        char *s = w34_hostedwork_56ec19_row_size_sql(json_get_str(in, "table", ""),
                                                     json_get_str(in, "prefix", ""));
        json_t *out = json_string(s ? s : "");
        free(s);
        return out;
    }
    if (!strcmp(func, "usage")) {
        return w34_hostedwork_56ec19_usage_sql(json_obj_get(in, "tables"));
    }
    if (!strcmp(func, "scope_disposition")) {
        char *s = w34_hostedwork_56ec19_scope_disposition(json_get_str(in, "table", ""),
                                                          json_obj_get(in, "row"),
                                                          json_obj_get(in, "current"));
        json_t *out = s ? json_string(s) : json_null();
        free(s);
        return out;
    }
    if (!strcmp(func, "guards")) {
        return w34_hostedwork_56ec19_guards_sql(json_obj_get(in, "tables"));
    }
    if (!strcmp(func, "invalid_guards")) {
        return w34_hostedwork_56ec19_invalid_guards_sql();
    }
    if (!strcmp(func, "save")) {
        const char *target = json_get_str(in, "target_install_id", NULL);
        const char *route = json_get_str(in, "route_generation", NULL);
        json_t *out = w34_hostedwork_56ec19_save(json_get_str(in, "table", ""),
                                                 json_obj_get(in, "record"),
                                                 json_get_str(in, "record_json", ""), target, route);
        return out ? out : json_null();
    }
    if (!strcmp(func, "budget")) {
        return json_bool(w34_hostedwork_56ec19_budget_refused(in) ? true : false);
    }
    if (!strcmp(func, "read_history")) {
        const json_t *epoch = json_obj_get(in, "epoch");
        return w34_hostedwork_56ec19_read_history(json_obj_get(in, "rows"),
                                                  json_get_str(in, "gateway_id", ""),
                                                  epoch && epoch->type == JSON_NUMBER ? epoch->num_val : 0.0);
    }
    if (!strcmp(func, "prefix")) {
        const json_t *authority = json_obj_get(in, "authority");
        const json_t *seq = json_obj_get(in, "seq");
        return w34_hostedwork_56ec19_prefix_check(json_obj_get(in, "spans"), json_obj_get(in, "events"),
                                                  seq && seq->type == JSON_NUMBER ? seq->num_val : 0.0,
                                                  json_get_str(authority, "gateway_id", ""),
                                                  json_get_num(authority, "epoch", 0.0));
    }
    if (!strcmp(func, "capture_tasks")) {
        return w34_hostedwork_56ec19_capture_tasks(in);
    }
    if (!strcmp(func, "constants")) {
        return w34_hostedwork_56ec19_constants();
    }
    return json_string("UNKNOWN_FUNC");
}

int main(void) {
    char line[1048576];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        size_t i;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        out = dispatch(func, in);
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
