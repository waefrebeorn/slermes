/* t_port_w13_pluginscmd_56fc85.c — harness for src/hermes_cli/plugins_cmd_git.c */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_pluginscmd_56fc85.h"

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
        if (!strcmp(func, "pinned_revision")) {
            out = w13_pluginscmd_56fc85_pinned_revision(in);
        } else if (!strcmp(func, "pin_annotation")) {
            out = w13_pluginscmd_56fc85_pin_annotation(in);
        } else if (!strcmp(func, "normalize_exact_revision")) {
            out = w13_pluginscmd_56fc85_normalize_exact_revision(in);
        } else if (!strcmp(func, "scrub_git_url")) {
            out = w13_pluginscmd_56fc85_scrub_git_url(in);
        } else if (!strcmp(func, "canonical_source")) {
            out = w13_pluginscmd_56fc85_canonical_source(in);
        }
        printf("{");
        json_t *fv = json_string(func);
        emit_key("func", fv); json_free(fv);
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
