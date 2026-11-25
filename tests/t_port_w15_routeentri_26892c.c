#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_routeentri_26892c.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static const char *jstrd(const json_t *in, const char *k, const char *d) {
    const char *s = jstr(json_obj_get(in, k));
    return s ? s : d;
}
static json_t *str_or_null(char *s) {
    json_t *o = s ? json_string(s) : json_null();
    free(s);
    return o;
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
        if (!strcmp(func, "sanitize_prompt_pin")) {
            out = w15_routeentri_26892c_sanitize_prompt_pin(json_obj_get(in, "pin"));
            if (!out) out = json_null();
        } else if (!strcmp(func, "sanitize_model_override")) {
            out = w15_routeentri_26892c_sanitize_model_override(json_obj_get(in, "override"));
            if (!out) out = json_null();
        } else if (!strcmp(func, "is_path_unsafe")) {
            out = json_bool(w15_routeentri_26892c_is_path_unsafe(
                json_obj_get(in, "value"), json_get_bool(in, "strict", 1)) ? 1 : 0);
        } else if (!strcmp(func, "session_key_namespace")) {
            const json_t *p = json_obj_get(in, "profile");
            out = str_or_null(w15_routeentri_26892c_session_key_namespace(jstr(p)));
        } else if (!strcmp(func, "build_session_key")) {
            /* The live oracle constructs SessionSource from the top-level case
             * fields; mirror that by collecting them into a source object. */
            json_t *src = json_object();
            for (size_t i = 0; i < json_object_size(in); ++i) {
                const char *k = json_object_get_key_at(in, i);
                if (!k || !strcmp(k, "func") || !strcmp(k, "case") ||
                    !strcmp(k, "group_sessions_per_user") ||
                    !strcmp(k, "thread_sessions_per_user") || !strcmp(k, "profile")) continue;
                json_set(src, k, json_copy(json_object_get_at(in, i)));
            }
            out = str_or_null(w15_routeentri_26892c_build_session_key(
                src,
                json_get_bool(in, "group_sessions_per_user", 1),
                json_get_bool(in, "thread_sessions_per_user", 0),
                jstrd(in, "profile", NULL)));
            json_free(src);
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
