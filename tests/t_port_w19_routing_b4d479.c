#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_routing_b4d479.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *opt_s(json_t *o, const char *k) {
    json_t *v = o ? json_obj_get(o, k) : NULL;
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static int flag(json_t *o, const char *k, int def) {
    json_t *v = o ? json_obj_get(o, k) : NULL;
    if (!v || v->type == JSON_NULL) return def;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    return def;
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
        if (!strcmp(func, "route_namespace")) {
            out = json_string(w19_routing_b4d479_route_namespace(opt_s(in, "session_key")));
        } else if (!strcmp(func, "normalize_topic_profile_name")) {
            out = json_string(w19_routing_b4d479_normalize_topic_profile_name(
                opt_s(in, "profile_name")));
        } else if (!strcmp(func, "route")) {
            out = w19_routing_b4d479_route(json_obj_get(in, "members"), flag(in, "keep_falsy", 0));
        } else if (!strcmp(func, "gateway_runtime")) {
            out = w19_routing_b4d479_gateway_runtime(json_obj_get(in, "model_config"),
                                                     opt_s(in, "billing_provider"),
                                                     opt_s(in, "billing_base_url"));
        } else if (!strcmp(func, "resume_route")) {
            out = w19_routing_b4d479_resume_route(json_obj_get(in, "model_config"),
                                                  opt_s(in, "model"),
                                                  opt_s(in, "billing_provider"),
                                                  opt_s(in, "billing_base_url"));
        } else if (!strcmp(func, "is_empty")) {
            out = json_bool(w19_routing_b4d479_is_empty(json_obj_get(in, "runtime")) ? true : false);
        } else if (!strcmp(func, "constants")) {
            out = w19_routing_b4d479_constants();
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
            emit_key(k, json_object_get_at(in, i));
        }
        fputs("}\n", stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
