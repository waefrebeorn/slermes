#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_turnrespon_df1eeb.h"
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
        if (!strcmp(func, "codex_finish_reason")) {
            char *s = w11_turnrespon_df1eeb_codex_finish_reason(
                json_obj_get(in, "status"), json_obj_get(in, "incomplete_details"));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "derive_finish_reason")) {
            char *s = w11_turnrespon_df1eeb_derive_finish_reason(
                json_get_str(in, "api_mode", ""), json_obj_get(in, "status"),
                json_obj_get(in, "incomplete_details"),
                (int)json_get_num(in, "codex_tool_calls", 0),
                json_get_str(in, "transport_finish", "stop"),
                (int)json_get_num(in, "treat_stop_truncated", 0));
            out = json_string(s ? s : "");
            free(s);
        }
        printf("{"); emit_key("func", json_string(func)); fputs(",", stdout); emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout); emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        json_free(out); json_free(in);
    }
    return 0;
}
