/* JSON-lines harness for the w27_pluginapi_1215d9_p4 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_pluginapi_1215d9_p4.h"

typedef json_t *(*fn_t)(const json_t *);

static const struct {
    const char *name;
    fn_t fn;
} TABLE[] = {
    {"board_display_kwargs", w27p4_board_display_kwargs},
    {"resolve_project", w27p4_resolve_project},
    {"default_workspace_kind", w27p4_default_workspace_kind},
    {"annotate_board_meta", w27p4_annotate_board_meta},
    {"validated_profile_name", w27p4_validated_profile_name},
    {"orchestration_settings", w27p4_orchestration_settings},
    {"since_param", w27p4_since_param},
    {"ws_board", w27p4_ws_board},
    {"ws_board_live", w27p4_ws_board_live},
    {"list_boards_annotate", w27p4_list_boards_annotate},
    {"profile_roster", w27p4_profile_roster},
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
