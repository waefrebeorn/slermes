#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_releasecha_5f3af5.h"

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
        if (!strcmp(func, "name")) {
            out = w15_releasecha_5f3af5_validate_name(json_obj_get(in, "value"));
        } else if (!strcmp(func, "repository")) {
            out = w15_releasecha_5f3af5_validate_repository(json_obj_get(in, "value"));
        } else if (!strcmp(func, "sha256")) {
            out = w15_releasecha_5f3af5_require_sha256(json_obj_get(in, "value"));
        } else if (!strcmp(func, "commit")) {
            out = w15_releasecha_5f3af5_require_commit(json_obj_get(in, "value"));
        } else if (!strcmp(func, "build_prefix")) {
            out = w15_releasecha_5f3af5_build_prefix(json_obj_get(in, "value"));
        } else if (!strcmp(func, "channel_key")) {
            out = w15_releasecha_5f3af5_channel_key(json_obj_get(in, "value"));
        } else if (!strcmp(func, "package_versions")) {
            out = w15_releasecha_5f3af5_package_versions(json_get_str(in, "sequence", ""));
        } else if (!strcmp(func, "artifact_key")) {
            out = w15_releasecha_5f3af5_artifact_key(json_obj_get(in, "value"));
        } else if (!strcmp(func, "head")) {
            out = w15_releasecha_5f3af5_head(json_obj_get(in, "value"));
        } else if (!strcmp(func, "identity")) {
            out = w15_releasecha_5f3af5_validate_identity(json_obj_get(in, "value"));
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
