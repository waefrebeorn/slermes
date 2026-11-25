#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_sessclose.h"

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
        const json_t *live = json_obj_get(in, "live");
        const char *state = json_get_str(in, "execution_state", "");
        int history = json_get_bool(in, "history_mount", 0) ? 1 : 0;
        json_t *out = NULL;
        if (!strcmp(func, "closed_idle_unviewed")) {
            out = sessclose_closed_idle_unviewed(live, state,
                        json_obj_get(in, "target_ended_at"), history);
        } else if (!strcmp(func, "closed_and_unviewed")) {
            out = sessclose_closed_and_unviewed(live, state, history,
                        json_get_str(in, "target", ""),
                        json_get_bool(in, "receipt_raises", 0) ? 1 : 0,
                        json_get_bool(in, "no_row", 0) ? 1 : 0,
                        json_obj_get(in, "row_ended_at"));
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
