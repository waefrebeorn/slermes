/* t_port_w13_climodalm_3a2dee.c — oracle harness for hermes_cli/cli_modal_mixin.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_climodalm_3a2dee.h"

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
        if (!strcmp(func, "approval_choices")) {
            out = w13_climodalm_3a2dee_approval_choices(json_obj_get(in, "command"),
                json_get_bool(in, "allow_permanent", 1), json_get_bool(in, "allow_session", 1),
                json_get_bool(in, "smart_denied", 0));
        } else if (!strcmp(func, "normalize")) {
            out = w13_climodalm_3a2dee_normalize_choice(json_obj_get(in, "raw"),
                                                        json_obj_get(in, "choices"));
        } else if (!strcmp(func, "opening_phase")) {
            out = w13_climodalm_3a2dee_connection_opening_phase(json_obj_get(in, "target"));
        } else if (!strcmp(func, "split_skip")) {
            out = w13_climodalm_3a2dee_split_destructive_skip(json_obj_get(in, "cmd_text"));
        } else if (!strcmp(func, "connection_fields")) {
            out = w13_climodalm_3a2dee_connection_fields(json_obj_get(in, "target"));
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
