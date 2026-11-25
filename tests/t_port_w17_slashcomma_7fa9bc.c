/* t_port_w17_slashcomma_7fa9bc.c — JSON-lines harness for gateway/slash_commands_status.py port. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w17_slashcomma_7fa9bc.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *jstr(char *s) { json_t *o = json_string(s ? s : ""); free(s); return o; }

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "clean_str"))
            out = jstr(w17_slashcomma_7fa9bc_clean_str(json_obj_get(in, "value")));
        else if (!strcmp(func, "int_value"))
            out = json_number((double)w17_slashcomma_7fa9bc_int_value(json_obj_get(in, "value")));
        else if (!strcmp(func, "fmt"))
            out = jstr(w17_slashcomma_7fa9bc_fmt(json_obj_get(in, "n")));
        else if (!strcmp(func, "pct"))
            out = json_number(w17_slashcomma_7fa9bc_pct(json_get_num(in, "used", 0),
                                                        json_get_num(in, "total", 0)));
        else if (!strcmp(func, "clip"))
            out = jstr(w17_slashcomma_7fa9bc_clip(json_get_str(in, "text", ""),
                                                  (int)json_get_num(in, "limit", 0)));
        else if (!strcmp(func, "routes_for_default"))
            out = w17_slashcomma_7fa9bc_routes_for_default(
                json_obj_get(in, "default_route"), json_obj_get(in, "persisted_route"),
                json_obj_get(in, "session_row"));
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
