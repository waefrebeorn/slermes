/* JSON-lines harness for the w28_files_ee43ed_p3 gate
 * (query parsing / coercion / payload decoding). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w28_files_ee43ed_p3.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *dispatch(const json_t *in, const char *func) {
    if (!strcmp(func, "decode_data_url")) return w28_files_ee43ed_p3_decode_data_url(in);
    if (!strcmp(func, "chat_image_extension")) return w28_files_ee43ed_p3_chat_image_extension(json_obj_get(in, "data"));
    if (!strcmp(func, "decode_chat_image_upload")) return w28_files_ee43ed_p3_decode_chat_image_upload(in);
    if (!strcmp(func, "media_proxy_host_allowed")) {
        return json_bool(w28_files_ee43ed_p3_media_proxy_host_allowed(json_get_str(in, "host", "")));
    }
    if (!strcmp(func, "media_proxy_target")) return w28_files_ee43ed_p3_media_proxy_target(in);
    if (!strcmp(func, "media_proxy_content_type_ok")) {
        return json_bool(w28_files_ee43ed_p3_media_proxy_content_type_ok(json_get_str(in, "content_type", "")));
    }
    if (!strcmp(func, "media_proxy_redirect")) return w28_files_ee43ed_p3_media_proxy_redirect(in);
    if (!strcmp(func, "proxy_response_verdict")) return w28_files_ee43ed_p3_proxy_response_verdict(in);
    if (!strcmp(func, "profile_key")) {
        char out[256];
        w28_files_ee43ed_p3_profile_key(json_get_str(in, "profile", ""), out, sizeof(out));
        return json_string(out);
    }
    if (!strcmp(func, "default_cwd_placeholder")) {
        return json_bool(w28_files_ee43ed_p3_default_cwd_placeholder(json_get_str(in, "raw", "")));
    }
    if (!strcmp(func, "read_text_meta")) return w28_files_ee43ed_p3_read_text_meta(in);
    if (!strcmp(func, "media_suffix_verdict")) return w28_files_ee43ed_p3_media_suffix_verdict(in);
    return json_string("UNKNOWN_FUNC");
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        out = dispatch(in, func);
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        fputs(",", stdout);
        emit_key("case", json_copy(json_obj_get(in, "case")));
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func") || !strcmp(k, "case")) continue;
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
