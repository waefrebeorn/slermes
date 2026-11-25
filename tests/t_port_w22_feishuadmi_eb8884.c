/* JSON-lines harness for the w22_feishuadmi_eb8884 gate.
 * One request object per stdin line; one response per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash.
 * Links against the port object (src/plugins/platforms/feishu/...o) + libjson. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w22_feishuadmi_eb8884.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

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
        if (!strcmp(func, "warning")) {
            out = w22_feishuadmi_eb8884_empty_allowlist_drop_warning(
                json_get_str(in, "chat_id", ""),
                json_obj_get(in, "group_rules"),
                json_get_str(in, "default_group_policy", "allowlist"),
                json_obj_get(in, "allowed_group_users"));
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
