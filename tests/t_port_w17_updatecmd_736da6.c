#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w17_updatecmd_736da6.h"

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

        if (!strcmp(func, "marker")) {
            char *s = w17_updatecmd_736da6_planned_stop_marker_path(json_get_str(in, "path", ""));
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "prefix")) {
            char *s = w17_updatecmd_736da6_lower_dir_prefix(json_get_str(in, "path", ""));
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "pids")) {
            out = w17_updatecmd_736da6_pending_relaunch_pids(
                json_obj_get(in, "profiles"), json_obj_get(in, "unmapped"), json_obj_get(in, "alive"));
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
