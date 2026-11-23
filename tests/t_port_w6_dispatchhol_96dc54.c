/* t_port_w6_dispatchhol_96dc54.c — JSON-lines harness for cron/dispatch_hold.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_dispatchhol_96dc54.h"

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

        const json_t *jc = json_obj_get(in, "content");
        const char *content = (jc && jc->type == JSON_STRING) ? jc->str_val : NULL;
        if (!strcmp(func, "read")) {
            out = w6_dispatchhol_96dc54_read(content);
        } else if (!strcmp(func, "grace_starts_at")) {
            struct json_t *held = w6_dispatchhol_96dc54_read(content);
            double due = json_get_num(in, "due_at", 0.0);
            double grace = json_get_num(in, "grace", 0.0);
            out = json_number(w6_dispatchhol_96dc54_grace_starts_at_ex(held, due, grace));
            json_free(held);
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
