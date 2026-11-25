/* JSON-lines harness for the w22_lockowners_a9d36b gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w22_lockowners_a9d36b.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "describe_range")) {
            char *s = w22_lockowners_a9d36b_describe_range(
                json_get_str(in, "sidecar", NULL),
                (long long)json_get_num(in, "start", 0),
                (long long)json_get_num(in, "end", 0));
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "parse_locks")) {
            out = w22_lockowners_a9d36b_parse_proc_locks(
                json_get_str(in, "text", NULL), json_obj_get(in, "inodes"));
        } else if (!strcmp(func, "warning")) {
            char *s = w22_lockowners_a9d36b_warning_message(
                json_get_str(in, "path_name", NULL),
                json_get_num(in, "patience", 0),
                json_obj_get(in, "holders"));
            out = s ? json_string(s) : json_null();
            free(s);
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
