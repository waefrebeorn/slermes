#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_kanbandbn_8ba814.h"

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
        if (!strcmp(func, "sub_key")) {
            out = w15_kanbandbn_8ba814_sub_key(json_obj_get(in, "task_id"),
                        json_obj_get(in, "platform"), json_obj_get(in, "chat_id"),
                        json_obj_get(in, "thread_id"));
        } else if (!strcmp(func, "encode_meta")) {
            out = w15_kanbandbn_8ba814_encode_notify_delivery_metadata(json_obj_get(in, "metadata"));
        } else if (!strcmp(func, "decode_meta")) {
            out = w15_kanbandbn_8ba814_decode_notify_delivery_metadata(json_obj_get(in, "raw"));
        } else if (!strcmp(func, "profile_filter")) {
            out = w15_kanbandbn_8ba814_notify_profile_filter(
                        json_obj_get(in, "notifier_profiles"),
                        json_get_bool(in, "include_unowned", false));
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
