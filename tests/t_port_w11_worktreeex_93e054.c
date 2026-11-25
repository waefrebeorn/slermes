#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_worktreeex_93e054.h"
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
        int has_tty = json_get_bool(in, "has_tty", false) ? 1 : 0;
        int in_use = json_get_bool(in, "still_in_use", false) ? 1 : 0;
        int err = json_get_bool(in, "curses_error", false) ? 1 : 0;
        json_t *out = NULL;
        if (!strcmp(func, "worktree_state")) {
            const char *s = w11_worktreeex_93e054_worktree_state(json_obj_get(in, "info"),
                        json_get_bool(in, "is_dir", false) ? 1 : 0,
                        json_get_bool(in, "is_dirty", false) ? 1 : 0,
                        json_get_bool(in, "has_unpushed", false) ? 1 : 0);
            out = s ? json_string(s) : NULL;
        } else if (!strcmp(func, "ask_exit_choice")) {
            const char *c = w11_worktreeex_93e054_ask_exit_choice(json_obj_get(in, "info"),
                        has_tty, json_obj_get(in, "state"), in_use,
                        json_obj_get(in, "curses_idx"), err);
            out = c ? json_string(c) : NULL;
        }
        printf("{"); emit_key("func", json_string(func)); fputs(",", stdout); emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout); emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        fflush(stdout);
        json_free(out); json_free(in);
    }
    return 0;
}
