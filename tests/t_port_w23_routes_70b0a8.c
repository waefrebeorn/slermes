/* JSON-lines harness for the w23_routes_70b0a8 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_routes_70b0a8.h"

/* Python str() of a render argument: a number prints as an integer, a null or
 * an absent key prints as None (the f-string never checks). */
static char num_buf[4][64];
static int num_slot;
static const char *arg_text(const json_t *v) {
    char *buf;
    long long n;
    if (!v || v->type == JSON_NULL) return "None";
    if (v->type == JSON_STRING) return v->str_val;
    if (v->type == JSON_BOOL) return v->bool_val ? "True" : "False";
    if (v->type != JSON_NUMBER) return "None";
    buf = num_buf[num_slot];
    num_slot = (num_slot + 1) & 3;
    n = (long long)v->num_val;
    if ((double)n == v->num_val) { snprintf(buf, 64, "%lld", n); return buf; }
    snprintf(buf, 64, "%g", v->num_val);
    return buf;
}

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *take_str(char *s) {
    json_t *node = json_string(s ? s : "");
    free(s);
    return node;
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "in_window")) {
            out = take_str(w23_routes_70b0a8_in_window(arg_text(json_obj_get(in, "expr"))));
        } else if (!strcmp(func, "freshest_of")) {
            out = take_str(w23_routes_70b0a8_freshest_of(
                arg_text(json_obj_get(in, "activity")),
                arg_text(json_obj_get(in, "session_id_expr")),
                arg_text(json_obj_get(in, "started"))));
        } else if (!strcmp(func, "last_active")) {
            json_t *alias = json_obj_get(in, "alias");
            out = take_str(w23_routes_70b0a8_last_active(alias ? arg_text(alias) : NULL));
        } else if (!strcmp(func, "fence_key")) {
            out = take_str(w23_routes_70b0a8_fence_key(
                arg_text(json_obj_get(in, "namespace")),
                arg_text(json_obj_get(in, "session_id"))));
        } else if (!strcmp(func, "namespace")) {
            out = take_str(w23_routes_70b0a8_namespace(arg_text(json_obj_get(in, "session_key"))));
        } else if (!strcmp(func, "retired_params")) {
            out = w23_routes_70b0a8_retired_params(in);
        } else if (!strcmp(func, "constants")) {
            out = w23_routes_70b0a8_constants();
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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
