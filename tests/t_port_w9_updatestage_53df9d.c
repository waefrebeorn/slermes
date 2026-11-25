#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w9_updatestage_53df9d.h"

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
        if (!strcmp(func, "ui_present_in_log")) {
            out = w9_updatestage_53df9d_ui_present_in_log(json_get_str(in, "log_text", ""));
        } else if (!strcmp(func, "status_file")) {
            out = w9_updatestage_53df9d_status_file(
                json_get_str(in, "exported", ""),
                json_get_str(in, "marker_result", ""));
        } else if (!strcmp(func, "status_from_marker")) {
            out = w9_updatestage_53df9d_status_from_marker(
                json_get_str(in, "marker_text", ""),
                json_get_bool(in, "candidate_exists", 0) ? 1 : 0);
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
