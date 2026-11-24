#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_errors_45219f.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
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
        json_t *cn = json_obj_get(in, "code");
        int has_code = (cn && cn->type == JSON_NUMBER) ? 1 : 0;
        long long code = has_code ? (long long)cn->num_val : 0;
        if (!strcmp(func, "is_no_more_rows")) {
            out = json_bool(w19_errors_45219f_is_no_more_rows(json_get_str(in, "msg", "")));
        } else if (!strcmp(func, "is_transient")) {
            out = json_bool(w19_errors_45219f_is_transient(
                json_get_str(in, "kind", ""), has_code, code, json_get_str(in, "msg", "")));
        } else if (!strcmp(func, "is_malformed_schema")) {
            out = json_bool(w19_errors_45219f_is_malformed_schema(
                json_get_str(in, "kind", ""), json_get_str(in, "msg", "")));
        } else if (!strcmp(func, "is_fts_scoped")) {
            out = json_bool(w19_errors_45219f_is_fts_scoped(
                has_code, code, json_get_str(in, "msg", "")));
        } else if (!strcmp(func, "classify")) {
            out = w19_errors_45219f_classify(json_obj_get(in, "exc"));
        } else if (!strcmp(func, "compression_closed_msg")) {
            out = w19_errors_45219f_compression_closed_msg(json_get_str(in, "session_id", ""));
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
