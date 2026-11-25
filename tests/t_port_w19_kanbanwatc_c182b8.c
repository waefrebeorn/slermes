#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_kanbanwatc_c182b8.h"

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
        if (!strcmp(func, "resolve_settings")) {
            out = w19_kanbanwatc_c182b8_resolve_settings(
                json_obj_get(in, "kanban_cfg"),
                (long long)json_get_num(in, "default_failure_limit", 0),
                json_obj_get(in, "effective_max_in_progress"));
        } else if (!strcmp(func, "is_corrupt_error")) {
            int r = w19_kanbanwatc_c182b8_is_corrupt_error(
                json_get_bool(in, "kanban_corrupt", 0),
                json_get_bool(in, "sqlite_db_error", 0),
                json_get_str(in, "msg", ""));
            out = json_bool(r);
        } else if (!strcmp(func, "quarantine_lifted")) {
            out = w19_kanbanwatc_c182b8_quarantine_lifted(
                json_get_bool(in, "has_entry", 0),
                json_obj_get(in, "disabled_fp"),
                json_get_num(in, "disabled_at", 0),
                json_obj_get(in, "fingerprint"),
                json_get_num(in, "now_mono", 0));
        } else if (!strcmp(func, "log_spawn_results")) {
            out = w19_kanbanwatc_c182b8_log_spawn_results(json_obj_get(in, "results"));
        } else if (!strcmp(func, "board_slugs")) {
            out = w19_kanbanwatc_c182b8_board_slugs(
                json_obj_get(in, "boards"), json_get_str(in, "default_board", ""));
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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
