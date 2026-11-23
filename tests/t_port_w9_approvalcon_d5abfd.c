#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w9_approvalcon_d5abfd.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "normalize_mode")) {
            char *s = w9_approvalcon_d5abfd_normalize_mode(json_obj_get(in, "value"));
            out = s ? json_string(s) : json_null(); free(s);
        } else if (!strcmp(func, "format_window")) {
            char *s = w9_approvalcon_d5abfd_format_window(json_get_str(in, "seconds", "0"));
            out = s ? json_string(s) : json_null(); free(s);
        } else if (!strcmp(func, "binary_mode")) {
            char *s = w9_approvalcon_d5abfd_binary_mode(json_obj_get(in, "value"));
            out = s ? json_string(s) : json_null(); free(s);
        } else if (!strcmp(func, "unattended_platform")) {
            out = json_bool(w9_approvalcon_d5abfd_unattended_platform(json_get_str(in, "platform", "")) ? true : false);
        } else if (!strcmp(func, "prompt_authority")) {
            out = w9_approvalcon_d5abfd_prompt_authority(json_obj_get(in, "req"), json_obj_get(in, "env"),
                                       json_obj_get(in, "approvers"));
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
