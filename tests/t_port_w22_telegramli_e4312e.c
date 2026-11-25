/* JSON-lines harness for the w22_telegramli_e4312e gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w22_telegramli_e4312e.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *tri(int v) { return json_number((double)v); }

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "keys")) {
            out = w22_telegramli_e4312e_keys();
        } else if (!strcmp(func, "allowlist")) {
            out = w22_telegramli_e4312e_allowlist_decision(json_get_str(in, "user_id", NULL),
                                                           json_get_str(in, "env", NULL));
        } else if (!strcmp(func, "refresh")) {
            out = tri(w22_telegramli_e4312e_refresh_needed(json_get_str(in, "baseline_home", NULL),
                                                           json_get_str(in, "baseline_version", NULL),
                                                           json_get_str(in, "current_version", NULL)));
        } else if (!strcmp(func, "merge")) {
            out = w22_telegramli_e4312e_merge_extra(json_obj_get(in, "extra"),
                                                    json_obj_get(in, "platforms"),
                                                    json_get_str(in, "platform", NULL));
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
