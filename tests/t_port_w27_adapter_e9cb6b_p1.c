/* JSON-lines harness for the w27_adapter_e9cb6b_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_e9cb6b_p1.h"

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
        if (!strcmp(func, "bang_split")) {
            out = w27_adapter_bang_split(json_obj_get(in, "text"));
        } else if (!strcmp(func, "bang_candidates")) {
            out = w27_adapter_bang_candidates(json_obj_get(in, "name"));
        } else if (!strcmp(func, "reply_fallback_extract")) {
            out = w27_adapter_reply_fallback_extract(json_obj_get(in, "body"));
        } else if (!strcmp(func, "reply_fallback_strip")) {
            out = w27_adapter_reply_fallback_strip(json_obj_get(in, "body"));
        } else if (!strcmp(func, "reply_fallback_split")) {
            out = w27_adapter_reply_fallback_split(json_obj_get(in, "body"));
        } else if (!strcmp(func, "permanent_auth_error")) {
            out = w27_adapter_permanent_auth_error(json_obj_get(in, "errcode"),
                                                   json_obj_get(in, "http_status"));
        } else if (!strcmp(func, "utf8_len")) {
            out = w27_adapter_utf8_len(json_obj_get(in, "text"));
        } else if (!strcmp(func, "max_message_length")) {
            out = w27_adapter_max_message_length(json_obj_get(in, "raw"));
        } else if (!strcmp(func, "looks_like_transport_filename")) {
            out = w27_adapter_looks_like_transport_filename(
                json_obj_get(in, "text"),
                json_get_str(in, "kind", "image"),
                json_get_bool(in, "reject_spaces", false));
        } else if (!strcmp(func, "is_bare_media_filename")) {
            out = w27_adapter_is_bare_media_filename(json_obj_get(in, "msgtype"),
                                                     json_obj_get(in, "body"));
        } else if (!strcmp(func, "event_timestamp_seconds")) {
            out = w27_adapter_event_timestamp_seconds(json_obj_get(in, "ts"));
        } else if (!strcmp(func, "redact_url_for_log")) {
            out = w27_adapter_redact_url_for_log(json_obj_get(in, "url"));
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
