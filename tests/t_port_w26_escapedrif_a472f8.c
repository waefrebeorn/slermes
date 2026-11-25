/* JSON-lines harness for the w26_escapedrif_a472f8 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_escapedrif_a472f8.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *err_named(const char *name) {
    json_t *o = json_object();
    json_set(o, "error", json_string(name));
    return o;
}

static json_t *err_type(void) { return err_named("TypeError"); }

static int arr_all_str(json_t *v) {
    size_t i, n;
    if (!v || v->type != JSON_ARRAY) return 0;
    n = json_len(v);
    for (i = 0; i < n; i++) {
        json_t *e = json_get(v, i);
        if (!e || e->type != JSON_STRING) return 0;
    }
    return 1;
}

static int get_str_arg(json_t *in, const char *key, const char **out) {
    json_t *v = json_obj_get(in, key);
    if (!v || v->type != JSON_STRING) return 0;
    *out = v->str_val ? v->str_val : "";
    return 1;
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
        if (!strcmp(func, "halve")) {
            const char *s;
            out = get_str_arg(in, "s", &s) ? w26_escapedrif_a472f8_halve(s) : err_type();
        } else if (!strcmp(func, "runs_even")) {
            const char *s;
            out = get_str_arg(in, "s", &s) ? w26_escapedrif_a472f8_runs_even(s) : err_type();
        } else if (!strcmp(func, "strip_quotes")) {
            const char *s;
            out = get_str_arg(in, "s", &s) ? w26_escapedrif_a472f8_strip_quotes(s)
                                           : err_named("AttributeError");
        } else if (!strcmp(func, "quotes_all_spurious")) {
            json_t *sa = json_obj_get(in, "strings");
            out = arr_all_str(sa) ? w26_escapedrif_a472f8_quotes_all_spurious(sa) : err_type();
        } else if (!strcmp(func, "backslash_correct")) {
            const char *o, *n, *c;
            out = (get_str_arg(in, "old", &o) && get_str_arg(in, "new", &n) &&
                   get_str_arg(in, "content", &c))
                      ? w26_escapedrif_a472f8_backslash_correct(o, n, c) : err_type();
        } else if (!strcmp(func, "quote_correct")) {
            const char *o, *n, *c;
            out = (get_str_arg(in, "old", &o) && get_str_arg(in, "new", &n) &&
                   get_str_arg(in, "content", &c))
                      ? w26_escapedrif_a472f8_quote_correct(o, n, c) : err_type();
        } else if (!strcmp(func, "escape_drift")) {
            const char *o, *n, *c;
            out = (get_str_arg(in, "old", &o) && get_str_arg(in, "new", &n) &&
                   get_str_arg(in, "content", &c))
                      ? w26_escapedrif_a472f8_escape_drift(o, n, c) : err_type();
        } else if (!strcmp(func, "notes")) {
            out = w26_escapedrif_a472f8_notes();
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
