#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_qfile.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static const char *nullable_str(const json_t *in, const char *key) {
    json_t *v = json_obj_get(in, key);
    if (!v || v->type == JSON_NULL) return NULL;
    return v->type == JSON_STRING ? v->str_val : "";
}
static const char *action_name(int a) {
    if (a == QFILE_NOOP) return "noop";
    if (a == QFILE_SET) return "set";
    return "exit";
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

        if (!strcmp(func, "read")) {
            qfile_result_t res;
            qfile_read(nullable_str(in, "query_file"), nullable_str(in, "query_file") == NULL,
                       nullable_str(in, "query"), nullable_str(in, "query") == NULL,
                       json_get_str(in, "stdin_content", ""),
                       json_get_str(in, "file_content", ""),
                       json_get_bool(in, "read_ok", true),
                       json_get_str(in, "err_detail", ""), &res);
            out = json_object();
            json_set(out, "action", json_string(action_name(res.action)));
            json_set(out, "code", res.action == QFILE_EXIT ? json_number(2) : json_null());
            json_set(out, "query", res.query ? json_string(res.query) : json_null());
            json_set(out, "stderr", res.stderr_msg ? json_string(res.stderr_msg) : json_null());
            qfile_result_free(&res);
        } else if (!strcmp(func, "is_empty")) {
            out = json_bool(qfile_is_empty(json_get_str(in, "value", "")) != 0);
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
