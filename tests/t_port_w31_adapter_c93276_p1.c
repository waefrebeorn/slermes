/* JSON-lines harness for the w31_adapter_c93276_p1 gate (SimpleX inbound).
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w31_adapter_c93276_p1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "parse_comma_list")) {
            out = w31_adapter_c93276_p1_parse_comma_list(json_get_str(in, "value", ""));
        } else if (!strcmp(func, "redact_id")) {
            out = w31_adapter_c93276_p1_redact_id(json_obj_get(in, "value"));
        } else if (!strcmp(func, "path_suffix")) {
            out = w31_adapter_c93276_p1_path_suffix(json_get_str(in, "path", ""));
        } else if (!strcmp(func, "is_image_ext")) {
            out = w31_adapter_c93276_p1_is_image_ext(json_get_str(in, "ext", ""));
        } else if (!strcmp(func, "is_audio_ext")) {
            out = w31_adapter_c93276_p1_is_audio_ext(json_get_str(in, "ext", ""));
        } else if (!strcmp(func, "mime_for_ext")) {
            out = w31_adapter_c93276_p1_mime_for_ext(json_get_str(in, "ext", ""));
        } else if (!strcmp(func, "display_name")) {
            out = w31_adapter_c93276_p1_display_name(json_obj_get(in, "obj"),
                                                     json_get_str(in, "profile_key", "profile"),
                                                     json_obj_get(in, "default"));
        } else if (!strcmp(func, "may_receive_file")) {
            out = w31_adapter_c93276_p1_may_receive_file(in);
        } else if (!strcmp(func, "ts_iso")) {
            out = w31_adapter_c93276_p1_ts_iso(json_get_str(in, "ts", ""));
        } else if (!strcmp(func, "chat_item")) {
            out = w31_adapter_c93276_p1_chat_item(in);
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
