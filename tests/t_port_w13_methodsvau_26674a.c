/* t_port_w13_methodsvau_26674a.c — oracle harness for tui_gateway/methods_vault.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_methodsvau_26674a.h"

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
        if (!strcmp(func, "remove")) {
            out = methodsvau_remove(json_obj_get(in, "params"), json_obj_get(in, "removed"));
        } else if (!strcmp(func, "lock")) {
            out = methodsvau_lock();
        } else if (!strcmp(func, "add")) {
            out = methodsvau_add(json_obj_get(in, "params"), json_obj_get(in, "add_result"));
        } else if (!strcmp(func, "unlock")) {
            out = methodsvau_unlock(json_obj_get(in, "params"), json_obj_get(in, "backends"),
                                    json_obj_get(in, "exc_msg"));
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
