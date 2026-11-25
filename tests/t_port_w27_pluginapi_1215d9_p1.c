/* JSON-lines harness for the w27_pluginapi_1215d9_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_pluginapi_1215d9_p1.h"

typedef json_t *(*fn_t)(const json_t *);

static const struct {
    const char *name;
    fn_t fn;
} TABLE[] = {
    {"normalize_slug", w27p1_normalize_slug},
    {"resolve_board", w27p1_resolve_board},
    {"existing_board_slug", w27p1_existing_board_slug},
    {"placeholders", w27p1_placeholders},
    {"attachment_dict", w27p1_attachment_dict},
    {"task_dict", w27p1_task_dict},
    {"warnings_summary", w27p1_warnings_summary},
    {"attach_diagnostics", w27p1_attach_diagnostics},
    {"require_obj", w27p1_require_obj},
    {"require_ok", w27p1_require_ok},
    {"conflict", w27p1_conflict},
    {"map_errors", w27p1_map_errors},
    {"errors_to_500", w27p1_errors_to_500},
    {"link_tasks", w27p1_link_tasks},
    {"module_constants", w27p1_module_constants},
};

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        size_t i;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        for (i = 0; i < sizeof(TABLE) / sizeof(TABLE[0]); i++) {
            if (!strcmp(func, TABLE[i].name)) { out = TABLE[i].fn(in); break; }
        }
        if (!out) out = json_string("UNKNOWN_FUNC");
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (i = 0; i < json_object_size(in); i++) {
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
