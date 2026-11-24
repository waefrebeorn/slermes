#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_gatewaycha_19399b.h"

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
        if (!strcmp(func, "preview")) {
            out = w16_gatewaycha_19399b_pending_preview(json_obj_get(in, "text"));
        } else if (!strcmp(func, "quoted")) {
            out = w16_gatewaycha_19399b_quoted(json_obj_get(in, "row"),
                    json_get_str(in, "fallback", "lost turn"));
        } else if (!strcmp(func, "lines")) {
            out = w16_gatewaycha_19399b_pending_lines(json_obj_get(in, "pending"),
                    json_get_bool(in, "interactive", 1) ? 1 : 0);
        } else if (!strcmp(func, "lost")) {
            out = w16_gatewaycha_19399b_lost_admission(json_obj_get(in, "snapshot"),
                    json_get_str(in, "id", ""));
        } else if (!strcmp(func, "oneshot")) {
            out = w16_gatewaycha_19399b_oneshot_admission(json_obj_get(in, "receipt"));
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
