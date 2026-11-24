/* JSON-lines harness for the w29_hostedroom_8927a5_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w29_hostedroom_8927a5_p1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *opt_str(const json_t *in, const char *key) {
    const json_t *v = json_obj_get(in, key);
    if (!v || v->type != JSON_STRING) return NULL;
    return v->str_val;
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "task_update")) {
            out = w29_hostedroom_8927a5_p1_task_update(json_get_str(in, "set", ""),
                                                       json_get_str(in, "fence", ""));
        } else if (!strcmp(func, "generation_update")) {
            out = w29_hostedroom_8927a5_p1_generation_update(json_get_str(in, "set", ""),
                                                             json_get_str(in, "status", ""));
        } else if (!strcmp(func, "named_sql")) {
            out = w29_hostedroom_8927a5_p1_named_sql(json_get_str(in, "name", ""));
        } else if (!strcmp(func, "tasks_in_order_sql")) {
            out = w29_hostedroom_8927a5_p1_tasks_in_order_sql(opt_str(in, "status"));
        } else if (!strcmp(func, "task_index_sql")) {
            out = w29_hostedroom_8927a5_p1_task_index_sql(json_get_bool(in, "if_not_exists", false));
        } else if (!strcmp(func, "task_columns")) {
            out = w29_hostedroom_8927a5_p1_task_columns();
        } else if (!strcmp(func, "transition")) {
            out = w29_hostedroom_8927a5_p1_transition(json_get_str(in, "name", ""));
        } else if (!strcmp(func, "ddl")) {
            const char *kind = json_get_str(in, "kind", "");
            if (!strcmp(kind, "leases"))
                out = w29_hostedroom_8927a5_p1_leases_table_ddl();
            else
                out = w29_hostedroom_8927a5_p1_task_table_ddl(opt_str(in, "table"));
        } else if (!strcmp(func, "schema_sql")) {
            out = w29_hostedroom_8927a5_p1_schema_sql(json_get_bool(in, "objects_exist", false));
        } else if (!strcmp(func, "step_mode")) {
            out = w29_hostedroom_8927a5_p1_step_mode(json_get_bool(in, "objects_exist", false));
        } else if (!strcmp(func, "schema_objects_exist")) {
            out = w29_hostedroom_8927a5_p1_schema_objects_exist(json_obj_get(in, "tables"),
                                                                json_get_bool(in, "has_index", false));
        } else if (!strcmp(func, "validate_schema")) {
            out = w29_hostedroom_8927a5_p1_validate_schema(json_obj_get(in, "lease_columns"),
                                                           json_obj_get(in, "task_columns"),
                                                           json_get_bool(in, "lease_fk", false),
                                                           json_get_bool(in, "task_fk", false));
        } else if (!strcmp(func, "schema_ready")) {
            out = w29_hostedroom_8927a5_p1_schema_ready(in);
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
