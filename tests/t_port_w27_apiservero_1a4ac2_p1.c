/* JSON-lines harness for the w27_apiservero_1a4ac2_p1 gate (api_server_openai_routes
 * turn verdicts / status algebra).  One request object per stdin line; one response
 * line carrying the computed "out" plus every input key echoed back. */
#include "hermes_json.h"
#include "noc_w27_apiservero_1a4ac2_p1.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static int truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0];
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default: return 0;
    }
}

static int flag(const json_t *in, const char *key, int dflt) {
    json_t *v = json_obj_get(in, key);
    if (!v || v->type == JSON_NULL) return dflt;
    return truthy(v);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "result_flags")) {
            out = w27_apiservero_1a4ac2_p1_result_flags(json_obj_get(in, "result"));
        } else if (!strcmp(func, "budget_exhausted")) {
            out = w27_apiservero_1a4ac2_p1_budget_exhausted(json_obj_get(in, "result"));
        } else if (!strcmp(func, "finish_reason")) {
            json_t *ae = json_obj_get(in, "agent_error");
            int has_ae = ae && ae->type != JSON_NULL;
            out = json_string(w27_apiservero_1a4ac2_p1_finish_reason(
                flag(in, "completed", 1), flag(in, "is_partial", 0), flag(in, "is_failed", 0),
                json_get_str(in, "err_msg", NULL), has_ae, flag(in, "budget_exhausted", 0)));
        } else if (!strcmp(func, "response_status")) {
            out = w27_apiservero_1a4ac2_p1_response_status(json_obj_get(in, "result"));
        } else if (!strcmp(func, "hermes_extras")) {
            out = w27_apiservero_1a4ac2_p1_hermes_extras(
                flag(in, "completed", 1), flag(in, "is_partial", 0), flag(in, "is_failed", 0),
                json_obj_get(in, "err_msg"), json_get_str(in, "finish_reason", "stop"),
                flag(in, "budget_exhausted", 0));
        } else if (!strcmp(func, "settled")) {
            out = w27_apiservero_1a4ac2_p1_settled(json_obj_get(in, "record"));
        } else if (!strcmp(func, "fingerprint_keys")) {
            out = w27_apiservero_1a4ac2_p1_fingerprint_keys(json_obj_get(in, "body"));
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
        if (out) json_free(out);
        json_free(in);
    }
    return 0;
}
