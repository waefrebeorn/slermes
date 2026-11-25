/* t_port_connuse.c — oracle harness for tools/connectors/data_use.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_connuse.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static void record_from(const json_t *arr) {
    if (!arr || arr->type != JSON_ARRAY) return;
    for (size_t i = 0; i < arr->c.count; i++) {
        json_t *it = arr->c.items[i];
        if (it && it->type == JSON_STRING) connuse_record(it->str_val);
    }
}
static json_t *snapshot(void) {
    connuse_sort();
    json_t *a = json_array();
    for (size_t i = 0; i < connuse_count(); i++) json_append(a, json_string(connuse_at(i)));
    return a;
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        connuse_clear();

        json_t *out = NULL;
        if (!strcmp(func, "collect")) {
            connuse_begin();
            json_t *batches = json_obj_get(in, "batches");
            if (batches && batches->type == JSON_ARRAY)
                for (size_t i = 0; i < batches->c.count; i++) record_from(batches->c.items[i]);
            connuse_end();
            out = snapshot();
        } else if (!strcmp(func, "after_scope")) {
            connuse_begin();
            record_from(json_obj_get(in, "inside"));
            connuse_end();
            record_from(json_obj_get(in, "outside"));  /* no-op: sink reset */
            out = snapshot();
        } else if (!strcmp(func, "outside")) {
            record_from(json_obj_get(in, "requires")); /* no sink bound -> no-op */
            out = snapshot();
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
        connuse_clear();
        json_free(in);
    }
    return 0;
}
