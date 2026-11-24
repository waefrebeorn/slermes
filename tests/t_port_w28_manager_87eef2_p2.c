/* JSON-lines harness for the w28_manager_87eef2_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w28_manager_87eef2_p2.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *dispatch(const json_t *in, const char *func) {
    if (!strcmp(func, "coerce_init"))
        return w28_manager_87eef2_p2_coerce_init(in);
    if (!strcmp(func, "parse_shortstat"))
        return w28_manager_87eef2_p2_parse_shortstat(json_get_str(in, "stat_line", ""));
    if (!strcmp(func, "list_checkpoints"))
        return w28_manager_87eef2_p2_list_checkpoints(in);
    if (!strcmp(func, "ledger_load"))
        return w28_manager_87eef2_p2_ledger_load(json_get_str(in, "text", ""));
    if (!strcmp(func, "ledger_trim"))
        return w28_manager_87eef2_p2_ledger_trim(json_obj_get(in, "ledger"));
    if (!strcmp(func, "list_projects"))
        return w28_manager_87eef2_p2_list_projects(json_obj_get(in, "entries"));
    if (!strcmp(func, "format_list"))
        return w28_manager_87eef2_p2_format_list(json_obj_get(in, "checkpoints"),
                                                 json_get_str(in, "directory", ""));
    return json_string("UNKNOWN_FUNC");
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        out = dispatch(in, func);
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
