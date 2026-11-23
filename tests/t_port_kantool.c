/* t_port_kantool.c — oracle harness for tools/kanban_toolset_context.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_kantool.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static json_t *tri(int v) { return v < 0 ? json_null() : json_bool(v ? true : false); }

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");

        const char *ts[256]; size_t nts = 0;
        json_t *arr = json_obj_get(in, "toolsets");
        if (arr && arr->type == JSON_ARRAY) {
            for (size_t i = 0; i < arr->c.count && nts < 256; i++) {
                json_t *it = arr->c.items[i];
                ts[nts++] = (it && it->type == JSON_STRING) ? it->str_val : "";
            }
        }

        json_t *out = NULL;
        if (!strcmp(func, "selection")) {
            out = json_bool(kantool_explicit(ts, nts) ? true : false);
        } else if (!strcmp(func, "scoped")) {
            kantool_reset();
            int outside = kantool_requested();
            int explicit = kantool_explicit(ts, nts);
            int tok = kantool_enter(explicit);
            int inside = kantool_requested();
            kantool_exit(tok);
            int after = kantool_requested();
            json_t *o = json_object();
            json_set(o, "outside", tri(outside));
            json_set(o, "inside", tri(inside));
            json_set(o, "after", tri(after));
            out = o;
        } else if (!strcmp(func, "requested")) {
            out = tri(kantool_requested());
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
