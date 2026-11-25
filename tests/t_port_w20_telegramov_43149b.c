/* JSON-lines harness for the w20_telegramov_43149b gate.
 * One request object per stdin line; one response per line with "out" plus
 * every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_telegramov_43149b.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *finish(json_t *in, const char *func, json_t *out) {
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
    return out;
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        w20_telegramov_43149b_env e;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");

        memset(&e, 0, sizeof(e));
        e.adapter = json_obj_get(in, "adapter");
        e.split = json_obj_get(in, "split");
        e.first_edit = json_obj_get(in, "first_edit");
        e.continuations = json_obj_get(in, "continuations");
        e.metadata = json_obj_get(in, "metadata");
        e.finalize = json_get_bool(in, "finalize", 0) ? 1 : 0;

        if (!strcmp(func, "edit")) {
            out = w20_telegramov_43149b_edit_overflow_split(
                json_get_str(in, "chat_id", ""), json_get_str(in, "message_id", ""),
                json_get_str(in, "content", ""), &e);
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        finish(in, func, out);
        json_free(out);
        json_free(in);
    }
    return 0;
}
