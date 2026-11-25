#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_adapter_5d2c00.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *opt_s(json_t *o, const char *k) {
    json_t *v = o ? json_obj_get(o, k) : NULL;
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static double num(json_t *o, const char *k, double def) {
    json_t *v = o ? json_obj_get(o, k) : NULL;
    if (!v) return def;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    return def;
}

static int flag(json_t *o, const char *k, int def) {
    json_t *v = o ? json_obj_get(o, k) : NULL;
    if (!v || v->type == JSON_NULL) return def;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0] ? 1 : 0;
    return def;
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
        if (!strcmp(func, "constants")) {
            out = w19_adapter_5d2c00_constants();
        } else if (!strcmp(func, "ws_proxy")) {
            out = w19_adapter_5d2c00_ws_proxy(json_obj_get(in, "env"));
        } else if (!strcmp(func, "parse_json")) {
            out = w19_adapter_5d2c00_parse_json(opt_s(in, "raw"));
        } else if (!strcmp(func, "parse_gateway_session_key")) {
            out = w19_adapter_5d2c00_parse_gateway_session_key(opt_s(in, "session_key"));
        } else if (!strcmp(func, "is_authorized_interaction")) {
            out = w19_adapter_5d2c00_is_authorized_interaction(json_obj_get(in, "event"),
                                                              opt_s(in, "session_key"));
        } else if (!strcmp(func, "append_block")) {
            out = w19_adapter_5d2c00_append_block(opt_s(in, "text"), opt_s(in, "block"));
        } else if (!strcmp(func, "merge_quote_into")) {
            out = w19_adapter_5d2c00_merge_quote_into(opt_s(in, "text"),
                                                      opt_s(in, "quote_block"));
        } else if (!strcmp(func, "detect_message_type")) {
            out = w19_adapter_5d2c00_detect_message_type(json_obj_get(in, "media_urls"),
                                                         json_obj_get(in, "media_types"));
        } else if (!strcmp(func, "opt_str")) {
            out = w19_adapter_5d2c00_opt_str(json_obj_get(in, "value"));
        } else if (!strcmp(func, "is_voice_content_type")) {
            out = w19_adapter_5d2c00_is_voice_content_type(opt_s(in, "content_type"),
                                                           opt_s(in, "filename"));
        } else if (!strcmp(func, "guess_ext_from_data")) {
            out = w19_adapter_5d2c00_guess_ext_from_data(opt_s(in, "hex"));
        } else if (!strcmp(func, "looks_like_silk")) {
            out = w19_adapter_5d2c00_looks_like_silk(opt_s(in, "hex"));
        } else if (!strcmp(func, "rest_path")) {
            out = w19_adapter_5d2c00_rest_path(opt_s(in, "chat_type"), opt_s(in, "target_id"),
                                               opt_s(in, "endpoint"));
        } else if (!strcmp(func, "messages_path")) {
            out = w19_adapter_5d2c00_messages_path(opt_s(in, "chat_type"), opt_s(in, "target_id"));
        } else if (!strcmp(func, "build_text_body")) {
            out = w19_adapter_5d2c00_build_text_body(opt_s(in, "content"), opt_s(in, "reply_to"),
                                                     flag(in, "markdown", 0),
                                                     (long)num(in, "max_len", 4000.0),
                                                     (long)num(in, "msg_seq", 0.0));
        } else if (!strcmp(func, "is_url")) {
            out = w19_adapter_5d2c00_is_url(opt_s(in, "source"));
        } else if (!strcmp(func, "strip_at_mention")) {
            out = w19_adapter_5d2c00_strip_at_mention(opt_s(in, "content"));
        } else if (!strcmp(func, "entry_matches")) {
            out = w19_adapter_5d2c00_entry_matches(json_obj_get(in, "entries"), opt_s(in, "target"));
        } else if (!strcmp(func, "guess_chat_type")) {
            out = w19_adapter_5d2c00_guess_chat_type(json_obj_get(in, "chat_type_map"),
                                                     opt_s(in, "chat_id"));
        } else if (!strcmp(func, "parse_qq_timestamp")) {
            out = w19_adapter_5d2c00_parse_qq_timestamp(opt_s(in, "raw"));
        } else if (!strcmp(func, "resolve_stt_config")) {
            out = w19_adapter_5d2c00_resolve_stt_config(json_obj_get(in, "extra"),
                                                        json_obj_get(in, "env"));
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
            emit_key(k, json_object_get_at(in, i));
        }
        fputs("}\n", stdout);
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
