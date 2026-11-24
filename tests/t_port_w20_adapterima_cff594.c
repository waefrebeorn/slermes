/* JSON-lines harness for the w20_adapterima_cff594 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash.
 *
 * The port source is included by bare name (the repo's harness convention) so
 * the gate links against libjson alone. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_adapterima_cff594.h"
#include "plugins/platforms/slack/adapter_image_batch.c"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "collect")) {
            out = w20_adapterima_cff594_collect(in);
        } else if (!strcmp(func, "ext_for")) {
            out = json_string(w20_adapterima_cff594_ext_for(json_get_str(in, "content_type", "")));
        } else if (!strcmp(func, "unquote")) {
            char *s = w20_adapterima_cff594_unquote(json_get_str(in, "value", ""));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "basename")) {
            char *s = w20_adapterima_cff594_basename(json_get_str(in, "path", ""));
            out = json_string(s ? s : "");
            free(s);
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
