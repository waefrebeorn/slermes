#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_cronenv.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

/* Python str() of a scope value: string stays, number -> decimal, bool -> True/False.
 * NULL represents None. */
static char *jstr(json_t *v) {
    if (!v || v->type == JSON_NULL) return NULL;
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char t[64];
        snprintf(t, sizeof t, "%lld", (long long)v->num_val);
        return strdup(t);
    }
    return json_dumps(v, 0);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "cron_env_setting")) {
            const char *name = json_get_str(in, "name", "");
            const char *def = json_get_str(in, "default", "");
            int multiplex = json_get_bool(in, "multiplex", 0) ? 1 : 0;
            int has_scope = json_get_bool(in, "has_scope", 0) ? 1 : 0;
            char *env_val = jstr(json_obj_get(in, "env_val"));
            char *scope_val = NULL, *home_scope_val = NULL;
            json_t *scope = json_obj_get(in, "scope");
            if (scope && scope->type == JSON_OBJECT) scope_val = jstr(json_obj_get(scope, name));
            json_t *hs = json_obj_get(in, "home_scope");
            if (hs && hs->type == JSON_OBJECT) home_scope_val = jstr(json_obj_get(hs, name));
            char *r = cronenv_cron_env_setting(name, def, multiplex, has_scope,
                                              env_val, scope_val, home_scope_val);
            out = json_string(r ? r : "");
            free(r);
            free(env_val);
            free(scope_val);
            free(home_scope_val);
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
