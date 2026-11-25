/* t_port_w16_slashcomma_10adf9.c — JSON-lines harness for gateway/slash_commands_goals.py port. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_slashcomma_10adf9.h"

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
        if (!strcmp(func, "mgr_call"))
            out = w16_slashcomma_10adf9_mgr_call(json_get_str(in, "prefix", ""), json_obj_get(in, "result"),
                               json_get_str(in, "exc_class", NULL), json_get_str(in, "exc_msg", NULL));
        else if (!strcmp(func, "quiet_bool"))
            out = json_bool(w16_slashcomma_10adf9_quiet_bool(json_obj_get(in, "value"),
                                            json_get_bool(in, "raises", false)) ? 1 : 0);
        else if (!strcmp(func, "loop_route"))
            out = w16_slashcomma_10adf9_loop_route(json_obj_get(in, "src"));
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
