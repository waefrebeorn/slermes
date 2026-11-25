/* JSON-lines harness for the w23_editcheck_cd6717 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_editcheck_cd6717.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[1048576];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "consts")) {
            out = w23_editcheck_cd6717_consts();
        } else if (!strcmp(func, "wait")) {
            double now = json_get_num(in, "now", 0.0);
            double w = w23_editcheck_cd6717_inline_wait(json_obj_get(in, "started"), now);
            out = json_number(w);
        } else if (!strcmp(func, "digest")) {
            char *h = w23_editcheck_cd6717_digest(json_get_str(in, "text", NULL));
            out = json_string(h ? h : "");
            free(h);
        } else if (!strcmp(func, "key")) {
            char *h = w23_editcheck_cd6717_diag_key(json_obj_get(in, "diag"));
            out = json_string(h ? h : "");
            free(h);
        } else if (!strcmp(func, "shift")) {
            out = w23_editcheck_cd6717_shift_baseline(json_obj_get(in, "baseline"),
                                                      json_get_str(in, "pre", NULL),
                                                      json_get_str(in, "post", NULL));
        } else if (!strcmp(func, "introduced")) {
            out = w23_editcheck_cd6717_introduced(json_obj_get(in, "diags"),
                                                  json_obj_get(in, "baseline"),
                                                  json_get_str(in, "pre", NULL),
                                                  json_get_str(in, "post", NULL));
        } else if (!strcmp(func, "cache")) {
            out = w23_editcheck_cd6717_cache_ops(json_obj_get(in, "ops"));
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
