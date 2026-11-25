/* t_port_w12_pluginsled_05da3f.c — JSON-lines oracle harness for hermes_cli/plugins_ledger.py */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_pluginsled_05da3f.h"

static void emit_key(const char *key, json_t *v)
{
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *jstr(const json_t *n)
{
    return (n && n->type == JSON_STRING) ? (n->str_val ? n->str_val : "") : NULL;
}

int main(void)
{
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;

        if (!strcmp(func, "remove_identity") || !strcmp(func, "remove_callback")) {
            size_t ti = (size_t)json_get_num(in, "target_index", 0);
            const json_t *items = json_obj_get(in, "items");
            out = !strcmp(func, "remove_identity")
                ? w12_pluginsled_05da3f_remove_identity(items, ti)
                : w12_pluginsled_05da3f_remove_callback(items, ti);
        } else if (!strcmp(func, "restore_mapping")) {
            out = w12_pluginsled_05da3f_restore_mapping(jstr(json_obj_get(in, "map_value")),
                                      jstr(json_obj_get(in, "current")),
                                      jstr(json_obj_get(in, "previous")));
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
