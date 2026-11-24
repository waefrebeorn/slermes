#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_adapterexe_bc6b62.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static int jbool(json_t *in, const char *key, int def) {
    json_t *v = json_obj_get(in, key);
    if (!v || v->type == JSON_NULL) return def;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0];
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
        if (!strcmp(func, "init_buttons")) {
            out = w19_adapterexe_bc6b62_init_buttons(
                jbool(in, "allow_once", 1), jbool(in, "allow_session", 1),
                jbool(in, "allow_permanent", 1), jbool(in, "smart_denied", 0));
        } else if (!strcmp(func, "check_auth")) {
            out = w19_adapterexe_bc6b62_check_auth(
                jbool(in, "base_ok", 1), jbool(in, "require_admin", 0),
                json_obj_get(in, "admin_user_ids"), json_get_str(in, "uid", ""));
        } else if (!strcmp(func, "footer_key")) {
            const char *r = w19_adapterexe_bc6b62_footer_key(json_get_str(in, "reason", ""));
            out = r ? json_string(r) : json_null();
        } else if (!strcmp(func, "resolve_outcome")) {
            out = w19_adapterexe_bc6b62_resolve_outcome(
                jbool(in, "gate_ok", 1), jbool(in, "refusal", 0), jbool(in, "resolve_raises", 0),
                (int)json_get_num(in, "count", 0.0), json_get_str(in, "request_id", "")[0] != 0,
                json_get_str(in, "color", "green"));
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
