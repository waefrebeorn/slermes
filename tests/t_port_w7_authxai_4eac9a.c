/* Port gate harness for hermes_cli/auth_xai.py (gate w7_authxai_4eac9a). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_authxai_4eac9a.h"
#include "hermes_cli/auth_xai.c"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *jstr(const json_t *in, const char *k) {
    json_t *v = json_obj_get(in, k);
    return (v && v->type == JSON_STRING) ? v->str_val : "";
}

static json_t *jval(const json_t *in, const char *k) {
    json_t *v = json_obj_get(in, k);
    return v ? v : json_null();
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
        if (!strcmp(func, "clean")) {
            out = w7_authxai_clean(jval(in, "value"));
        } else if (!strcmp(func, "token_pair")) {
            out = w7_authxai_token_pair(json_obj_get(in, "tokens"));
        } else if (!strcmp(func, "state_ok")) {
            out = w7_authxai_state_has_usable_tokens(json_obj_get(in, "state"));
        } else if (!strcmp(func, "origin")) {
            out = w7_authxai_is_xai_origin_host(jstr(in, "host"));
        } else if (!strcmp(func, "url_problem")) {
            out = w7_authxai_url_problem(jstr(in, "url"));
        } else if (!strcmp(func, "jwt_exp")) {
            out = w7_authxai_jwt_exp(jstr(in, "token"));
        } else if (!strcmp(func, "tokens_from")) {
            out = w7_authxai_tokens_from_payload(json_obj_get(in, "payload"),
                        jval(in, "access_token"), jstr(in, "fallback_refresh"));
        } else if (!strcmp(func, "validate_base")) {
            out = w7_authxai_validate_inference_base_url(jstr(in, "value"), jstr(in, "fallback"));
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
