#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_updatechan_790d05.h"

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
        if (!strcmp(func, "is_canary_tag")) {
            out = w12_updatechan_790d05_is_canary_tag(json_get_str(in, "tag", ""));
        } else if (!strcmp(func, "canary_timestamp")) {
            out = w12_updatechan_790d05_canary_timestamp(json_get_str(in, "tag", ""));
        } else if (!strcmp(func, "canary_tag_for_date")) {
            out = w12_updatechan_790d05_canary_tag_for_date(
                json_get_str(in, "version", ""), json_get_str(in, "date_utc", ""));
        } else if (!strcmp(func, "install_records")) {
            out = w12_updatechan_790d05_install_records(json_obj_get(in, "config"));
        } else if (!strcmp(func, "package_channel")) {
            out = w12_updatechan_790d05_package_channel(json_obj_get(in, "stamp"));
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
