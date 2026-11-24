/* JSON-lines harness for the w22_webassetmi_0a1f41 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w22_webassetmi_0a1f41.h"
#include "hermes_cli/web_asset_mime_types.c"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *opt_str(const char *s) {
    return s ? json_string(s) : NULL;
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
        if (!strcmp(func, "pairs")) {
            out = w22_webassetmi_0a1f41_pairs();
        } else if (!strcmp(func, "mime_for")) {
            out = opt_str(w22_webassetmi_0a1f41_mime_for(json_get_str(in, "ext", NULL)));
        } else if (!strcmp(func, "pinned_map")) {
            out = w22_webassetmi_0a1f41_pinned_map();
        } else if (!strcmp(func, "guess")) {
            out = opt_str(w22_webassetmi_0a1f41_guess(json_get_str(in, "url", NULL)));
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
