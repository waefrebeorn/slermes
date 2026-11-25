#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_turnfinal_45ceb6.h"
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
        if (!strcmp(func, "bare_marker")) {
            const char *nr = json_get_str(in, "needs_reply", "false");
            int needs = !strcmp(nr, "true") ? 1
                      : (!strcmp(nr, "false") || !strcmp(nr, "none")) ? 0 : 2;
            int r = w11_turnfinal_45ceb6_bare_marker_after_reply(
                needs,
                (int)json_get_num(in, "truncated_parts", 0),
                (int)json_get_num(in, "promoted", 0),
                (int)json_get_num(in, "interrupt_requested", 0),
                json_get_str(in, "reply", ""),
                (int)json_get_num(in, "reply_is_silence", 0),
                (int)json_get_num(in, "final_is_silence", 0));
            out = json_bool(r != 0);
        } else if (!strcmp(func, "absorb")) {
            json_t *msgs = json_copy(json_obj_get(in, "messages"));
            int had = 0;
            char *joined = w11_turnfinal_45ceb6_absorb_continuation_fragments(
                json_get_str(in, "final_response", ""), json_obj_get(in, "parts"), msgs, &had);
            out = json_object();
            json_set_hdr(out, "joined", joined ? joined : "");
            json_set(out, "had_parts", json_bool(had != 0));
            json_set(out, "messages", msgs ? msgs : json_array());
            free(joined);
        } else if (!strcmp(func, "pop_scaffolding")) {
            json_t *msgs = json_copy(json_obj_get(in, "messages"));
            int n = w11_turnfinal_45ceb6_pop_ephemeral_scaffolding(msgs);
            out = json_object();
            json_set(out, "count", json_number(n));
            json_set(out, "messages", msgs ? msgs : json_array());
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
