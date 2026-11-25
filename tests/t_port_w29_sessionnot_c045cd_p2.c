/* JSON-lines harness for the w29_sessionnot_c045cd_p2 gate (kanban line
 * rendering, completion/timeout suffixes, delegation display metadata, note
 * prefix).  One request object per stdin line; one response line carrying the
 * computed "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w29_sessionnot_c045cd_p2.h"

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
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "first_line")) {
            out = w29_sessionnot_c045cd_p2_kb_first_line(json_obj_get(in, "value"),
                                                         json_obj_get(in, "limit"));
        } else if (!strcmp(func, "kb_completed")) {
            out = w29_sessionnot_c045cd_p2_kb_completed(json_obj_get(in, "task"),
                                                        json_obj_get(in, "payload"),
                                                        json_obj_get(in, "title"));
        } else if (!strcmp(func, "kb_timed_out")) {
            out = w29_sessionnot_c045cd_p2_kb_timed_out(json_obj_get(in, "task"),
                                                        json_obj_get(in, "payload"),
                                                        json_obj_get(in, "title"));
        } else if (!strcmp(func, "kanban_text")) {
            out = w29_sessionnot_c045cd_p2_format_kanban_event_text(
                json_obj_get(in, "sub"), json_obj_get(in, "task"), json_obj_get(in, "ev"),
                json_obj_get(in, "board_slug"));
        } else if (!strcmp(func, "delegation_meta")) {
            out = w29_sessionnot_c045cd_p2_async_delegation_display_metadata(
                json_obj_get(in, "evt"), json_obj_get(in, "display_text"));
        } else if (!strcmp(func, "prepend_note")) {
            out = w29_sessionnot_c045cd_p2_prepend_note(json_obj_get(in, "run_message"),
                                                        json_obj_get(in, "note"));
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
