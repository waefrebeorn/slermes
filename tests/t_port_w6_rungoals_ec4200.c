#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_rungoals_ec4200.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static int py_truthy(json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0];
        case JSON_ARRAY:  return v->c.count > 0;
        case JSON_OBJECT: return v->c.count > 0;
        default: return 0;
    }
}
static char *py_str(json_t *v) {   /* Python str(v or "") for the scalar types in the fixture */
    if (!py_truthy(v)) return strdup("");
    switch (v->type) {
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            char b[64]; double d = v->num_val;
            if (d == (double)(long long)d) snprintf(b, sizeof b, "%lld", (long long)d);
            else snprintf(b, sizeof b, "%g", d);
            return strdup(b);
        }
        default: return strdup("");
    }
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
        if (!strcmp(func, "final_text")) {
            const char *kind = json_get_str(in, "agent_kind", "");
            json_t *value = json_obj_get(in, "value");
            char *text;
            if (!strcmp(kind, "str"))
                text = (value && value->type == JSON_STRING && value->str_val) ? strdup(value->str_val) : strdup("");
            else if (!strcmp(kind, "dict"))
                text = py_str(value ? json_obj_get(value, "final_response") : NULL);
            else
                text = strdup("");
            int has_event = json_get_bool(in, "has_event", 0) ? 1 : 0;
            json_t *sv = json_obj_get(in, "streamed");
            const char *streamed = (sv && sv->type == JSON_STRING) ? sv->str_val : NULL;
            out = json_string(w6_rungoals_ec4200_final_text(text, has_event, streamed));
            free(text);
        } else if (!strcmp(func, "goal_max_turns")) {
            json_t *goals = json_obj_get(in, "goals");
            int kind = 0; char valbuf[128] = "";
            if (goals && goals->type == JSON_OBJECT) {
                json_t *mt = json_obj_get(goals, "max_turns");
                if (mt) {
                    if (mt->type == JSON_NUMBER) { kind = 1; snprintf(valbuf, sizeof valbuf, "%g", mt->num_val); }
                    else if (mt->type == JSON_STRING) { kind = 2; snprintf(valbuf, sizeof valbuf, "%s", mt->str_val ? mt->str_val : ""); }
                    else if (mt->type == JSON_BOOL) { kind = 3; strcpy(valbuf, mt->bool_val ? "1" : "0"); }
                }
            }
            out = json_number((double)w6_rungoals_ec4200_goal_max_turns(kind, valbuf));
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
