#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_cli_d7cb5a.h"

static long long get_i64(json_t *in, const char *key, long long def) {
    json_t *v = json_obj_get(in, key);
    if (!v) return def;
    if (v->type == JSON_STRING) return strtoll(v->str_val, NULL, 10);
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    return def;
}
/* Pass a JSON-array field through as its serialized text (json_get_str would
 * substitute the default for non-string nodes). */
static char *field_json(json_t *in, const char *key, const char *def) {
    json_t *v = json_obj_get(in, key);
    return v ? json_dumps(v, 0) : strdup(def);
}
static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
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

        if (!strcmp(func, "fmt_bytes")) {
            char *s = pmcli_fmt_bytes(get_i64(in, "n", 0));
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "flag_error")) {
            char *ex = field_json(in, "extras", "[]");
            char *wt = field_json(in, "without", "[]");
            char *df = field_json(in, "defaults", "[]");
            char *dp = field_json(in, "droppable", "[]");
            char *s = pmcli_install_flag_error(
                json_get_str(in, "cross_target", ""), (int)get_i64(in, "n_names", 0),
                ex, json_get_bool(in, "tools_only", 0),
                json_get_bool(in, "trust_recorded", 0), json_get_bool(in, "test_env", 0),
                wt, df, dp);
            out = s ? json_string(s) : json_null();
            free(s); free(ex); free(wt); free(df); free(dp);
        } else if (!strcmp(func, "split_without")) {
            char *nm = field_json(in, "names", "[]");
            char *df = field_json(in, "defaults", "[]");
            char *dp = field_json(in, "droppable", "[]");
            char *s = pmcli_split_without(nm, df, dp);
            out = s ? json_parse(s, NULL) : json_null();
            free(s); free(nm); free(df); free(dp);
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        if (!out) out = json_null();

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
