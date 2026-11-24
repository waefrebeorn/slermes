/* t_port_w16_sessionvie_e7b625.c — JSON-lines harness for gateway/session_viewer_close.py port. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_sessionvie_e7b625.h"

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
        if (!strcmp(func, "seconds"))
            out = json_number(w16_sessionvie_e7b625_seconds(json_obj_get(in, "raw"), json_get_num(in, "default_value", 0)));
        else if (!strcmp(func, "section"))
            out = w16_sessionvie_e7b625_section(json_obj_get(in, "config"), json_get_str(in, "key", ""));
        else if (!strcmp(func, "pending_reason"))
            out = w16_sessionvie_e7b625_pending_reason(json_get_str(in, "record", NULL));
        else if (!strcmp(func, "is_reset_reason"))
            out = json_bool(w16_sessionvie_e7b625_is_reset_reason(json_get_str(in, "reason", NULL)) ? 1 : 0);
        else if (!strcmp(func, "is_orphan_reason"))
            out = json_bool(w16_sessionvie_e7b625_is_orphan_reason(json_get_str(in, "reason", NULL)) ? 1 : 0);
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
