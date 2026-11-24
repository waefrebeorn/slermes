#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w17_warningnot_af9536.h"

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
        json_t *ln = json_obj_get(in, "level");
        int level_null = (!ln || ln->type == JSON_NULL) ? 1 : 0;
        const char *level = (ln && ln->type == JSON_STRING) ? ln->str_val : "";
        int diag = json_get_bool(in, "diag", false) ? 1 : 0;
        int internal = json_get_bool(in, "internal", false) ? 1 : 0;
        const char *platform = json_get_str(in, "platform", "");
        json_t *uc = json_obj_get(in, "user_config");
        json_t *dm = json_obj_get(in, "display_metadata");
        if (!json_is_object(uc)) uc = NULL;
        if (!json_is_object(dm)) dm = NULL;
        if (!strcmp(func, "is_warning_status")) {
            out = json_bool(w17_warningnot_af9536_is_warning_status(
                              json_get_str(in, "event_type", ""), diag) ? true : false);
        } else if (!strcmp(func, "is_diagnostic_notice")) {
            out = json_bool(w17_warningnot_af9536_is_diagnostic_notice(
                              level, level_null, json_get_str(in, "key", "")) ? true : false);
        } else if (!strcmp(func, "diagnostic_metadata")) {
            out = w17_warningnot_af9536_diagnostic_metadata(internal, json_obj_get(in, "metadata"));
        } else if (!strcmp(func, "warning_notifications_enabled")) {
            out = json_bool(w17_warningnot_af9536_warning_notifications_enabled(platform, uc) ? true : false);
        } else if (!strcmp(func, "diagnostic_turn_muted")) {
            out = json_bool(w17_warningnot_af9536_diagnostic_turn_muted(dm, platform, uc) ? true : false);
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
