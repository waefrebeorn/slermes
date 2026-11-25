#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_browsersess_acacf0.h"

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
        if (!strcmp(func, "is_local")) {
            out = json_bool(w8_browsersess_acacf0_is_local_sidecar_key(json_get_str(in, "key", "")) ? true : false);
        } else if (!strcmp(func, "owner")) {
            char *r = w8_browsersess_acacf0_owner_task_id(json_get_str(in, "key", ""));
            out = json_string(r ? r : ""); free(r);
        } else if (!strcmp(func, "session_record")) {
            out = w8_browsersess_acacf0_session_record_ex(json_get_str(in, "prefix", ""),
                        json_obj_get(in, "cdp_url"), json_obj_get(in, "features"),
                        json_get_str(in, "hex10", ""));
        } else if (!strcmp(func, "publish")) {
            out = w8_browsersess_acacf0_publish(json_get_str(in, "cache_key", ""),
                        json_obj_get(in, "session_info"), json_get_str(in, "home_key", ""));
        } else if (!strcmp(func, "shares") || !strcmp(func, "human_holds_shared")) {
            const struct json_t *si = json_obj_get(in, "session_info");
            const char *disp = json_get_str(in, "display", "");
            int hh = json_get_bool(in, "human_holds", false) ? 1 : 0;
            if (!strcmp(func, "shares")) out = json_bool(w8_browsersess_acacf0_shares_bot_desktop_ex(si, disp, hh) ? true : false);
            else out = json_bool(w8_browsersess_acacf0_human_holds_shared_ex(si, disp, hh) ? true : false);
        } else if (!strcmp(func, "constants")) {
            out = w8_browsersess_acacf0_constants();
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