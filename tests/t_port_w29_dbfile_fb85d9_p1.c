/* JSON-lines harness for the w29_dbfile_fb85d9_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w29_dbfile_fb85d9_p1.h"

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
        if (!strcmp(func, "header")) {
            out = w29_dbfile_fb85d9_p1_header(json_get_str(in, "hex", ""));
        } else if (!strcmp(func, "appid")) {
            out = w29_dbfile_fb85d9_p1_appid(in);
        } else if (!strcmp(func, "sidecars")) {
            out = w29_dbfile_fb85d9_p1_sidecars(in);
        } else if (!strcmp(func, "watched")) {
            out = w29_dbfile_fb85d9_p1_watched(in);
        } else if (!strcmp(func, "unlinked_identity")) {
            out = w29_dbfile_fb85d9_p1_unlinked_identity(in);
        } else if (!strcmp(func, "unlinked_fd")) {
            out = w29_dbfile_fb85d9_p1_unlinked_fd(in);
        } else if (!strcmp(func, "own_fd")) {
            out = w29_dbfile_fb85d9_p1_own_fd(in);
        } else if (!strcmp(func, "oplink")) {
            out = w29_dbfile_fb85d9_p1_oplink(in);
        } else if (!strcmp(func, "holder_pids")) {
            out = w29_dbfile_fb85d9_p1_holder_pids(in);
        } else if (!strcmp(func, "canon")) {
            out = w29_dbfile_fb85d9_p1_canon(json_get_str(in, "path", ""));
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
