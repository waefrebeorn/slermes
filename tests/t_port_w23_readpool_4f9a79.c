/* JSON-lines harness for the w23_readpool_4f9a79 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_readpool_4f9a79.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
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
        if (!strcmp(func, "consts")) {
            out = w23_readpool_4f9a79_consts();
        } else if (!strcmp(func, "fd_headroom")) {
            out = w23_readpool_4f9a79_fd_headroom(in);
        } else if (!strcmp(func, "fd_soft_limit")) {
            out = w23_readpool_4f9a79_fd_soft_limit(in);
        } else if (!strcmp(func, "fd_count")) {
            out = w23_readpool_4f9a79_fd_count(in);
        } else if (!strcmp(func, "budget_key")) {
            out = w23_readpool_4f9a79_budget_key(in);
        } else if (!strcmp(func, "acquire")) {
            out = w23_readpool_4f9a79_acquire(in);
        } else if (!strcmp(func, "register")) {
            out = w23_readpool_4f9a79_register(in);
        } else if (!strcmp(func, "note_exhausted")) {
            out = w23_readpool_4f9a79_note_exhausted(in);
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
        json_free(out);
        json_free(in);
    }
    return 0;
}
