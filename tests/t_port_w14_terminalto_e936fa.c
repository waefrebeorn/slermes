#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w14_terminalto_e936fa.h"

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
        if (!strcmp(func, "profile_container_key")) {
            out = w14_terminalto_e936fa_profile_container_key(json_obj_get(in, "profile"));
        } else if (!strcmp(func, "is_host_cwd")) {
            out = w14_terminalto_e936fa_is_host_cwd(json_obj_get(in, "path"));
        } else if (!strcmp(func, "is_windows_drive_path")) {
            out = w14_terminalto_e936fa_is_windows_drive_path(json_obj_get(in, "path"));
        } else if (!strcmp(func, "host_path_key")) {
            out = w14_terminalto_e936fa_host_path_key(json_obj_get(in, "path"));
        } else if (!strcmp(func, "translate")) {
            out = w14_terminalto_e936fa_translate_mounted_host_path(
                json_obj_get(in, "path"), json_obj_get(in, "host_root"),
                json_obj_get(in, "container_root"));
        } else if (!strcmp(func, "is_unusable_cwd")) {
            out = w14_terminalto_e936fa_is_unusable_container_cwd(json_obj_get(in, "cwd"));
        } else if (!strcmp(func, "cwd_follows_host_mount")) {
            out = w14_terminalto_e936fa_cwd_follows_host_mount(
                json_obj_get(in, "cwd"), json_obj_get(in, "mount"));
        } else if (!strcmp(func, "tenv_bool")) {
            out = w14_terminalto_e936fa_tenv_bool(json_obj_get(in, "value"));
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
