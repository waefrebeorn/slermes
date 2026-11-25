/* JSON-lines harness for the w20_adaptervau_8ec608 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_adaptervau_8ec608.h"

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
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "frame_kept")) {
            out = json_bool(w20_adaptervau_8ec608_frame_kept(
                json_get_str(in, "message", "")) ? 1 : 0);
        } else if (!strcmp(func, "truncate_utf16")) {
            out = w20_adaptervau_8ec608_truncate_utf16(
                json_get_str(in, "text", ""), (int)json_get_num(in, "limit", 0.0));
        } else if (!strcmp(func, "check_auth")) {
            out = json_bool(w20_adaptervau_8ec608_check_auth(
                json_get_bool(in, "admitted", 0), json_obj_get(in, "interaction_user_id"),
                json_get_str(in, "user_id", "")) ? 1 : 0);
        } else if (!strcmp(func, "submit_plan")) {
            out = w20_adaptervau_8ec608_submit_plan(
                json_get_str(in, "code", ""), json_get_bool(in, "admitted", 0),
                json_get_bool(in, "accepted", 0));
        } else if (!strcmp(func, "prompt_content")) {
            out = w20_adaptervau_8ec608_prompt_content(json_get_str(in, "body", ""),
                                                       json_get_str(in, "hint", NULL));
        } else if (!strcmp(func, "input_id")) {
            out = json_string(w20_adaptervau_8ec608_input_id());
        } else if (!strcmp(func, "limits")) {
            out = json_object();
            json_set(out, "modal", json_number(w20_adaptervau_8ec608_modal_text_limit()));
            json_set(out, "hint", json_number(w20_adaptervau_8ec608_hint_limit()));
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
