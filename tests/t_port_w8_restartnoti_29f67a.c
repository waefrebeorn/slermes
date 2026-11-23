/* Port gate harness for gateway/restart_notice.py (gate w8_restartnoti_29f67a). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_restartnoti_29f67a.h"

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
        if (!strcmp(func, "marker_path")) {
            json_t *home = json_obj_get(in, "home");
            const char *hs = (home && home->type == JSON_STRING) ? home->str_val : NULL;
            out = restartnoti_planned_restart_marker_path(hs, json_get_str(in, "default_home", ""));
        } else if (!strcmp(func, "marker_payload")) {
            out = restartnoti_marker_payload(json_get_num(in, "now", 0.0),
                    json_get_bool(in, "via_service", false) ? 1 : 0,
                    json_get_bool(in, "detached", false) ? 1 : 0);
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
