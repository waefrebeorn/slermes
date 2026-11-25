#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_streamcons_39b4a2.h"

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

static int has_num(json_t *o, const char *k) {
    json_t *v = o ? json_obj_get(o, k) : NULL;
    return v && v->type == JSON_NUMBER;
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
        if (!strcmp(func, "edit_skipped")) {
            json_t *res = json_object();
            json_set(res, "raw_response", json_copy(json_obj_get(in, "raw_response")));
            out = w19_streamcons_39b4a2_edit_skipped(res);
            json_free(res);
        } else if (!strcmp(func, "visible_prefix")) {
            out = w19_streamcons_39b4a2_visible_prefix(opt_s(in, "last_sent_text"),
                                                       opt_s(in, "cursor"));
        } else if (!strcmp(func, "continuation_text")) {
            out = w19_streamcons_39b4a2_continuation_text(
                opt_s(in, "final_text"), opt_s(in, "fallback_prefix"), opt_s(in, "last_sent_text"),
                opt_s(in, "cursor"), flag(in, "fragment_can_be_replaced", 0));
        } else if (!strcmp(func, "prose_resume_point")) {
            out = w19_streamcons_39b4a2_prose_resume_point(opt_s(in, "final_text"),
                                                           (long)num(in, "cut", 0.0));
        } else if (!strcmp(func, "fragment_can_be_replaced")) {
            out = w19_streamcons_39b4a2_fragment_can_be_replaced(opt_s(in, "message_id"),
                                                                 flag(in, "preserve_partial", 0),
                                                                 flag(in, "adapter_can_delete", 0));
        } else if (!strcmp(func, "send_failure_may_have_delivered")) {
            out = w19_streamcons_39b4a2_send_failure_may_have_delivered(
                flag(in, "retryable", 0), flag(in, "delivery_uncertain", 0), opt_s(in, "error"),
                opt_s(in, "exc_name"));
        } else if (!strcmp(func, "is_flood_error")) {
            out = w19_streamcons_39b4a2_is_flood_error(opt_s(in, "error"));
        } else if (!strcmp(func, "flood_retry_delay")) {
            out = w19_streamcons_39b4a2_flood_retry_delay(opt_s(in, "error"),
                                                          has_num(in, "retry_after"),
                                                          num(in, "retry_after", 0.0),
                                                          num(in, "max_wait", 30.0));
        } else if (!strcmp(func, "adopt_partial_head")) {
            json_t *res = json_object();
            json_set(res, "raw_response", json_copy(json_obj_get(in, "raw_response")));
            json_set(res, "message_id", json_copy(json_obj_get(in, "result_message_id")));
            out = w19_streamcons_39b4a2_adopt_partial_head(res, opt_s(in, "text"));
            json_free(res);
        } else if (!strcmp(func, "raw_message_limit")) {
            const char *cap_kind = opt_s(in, "cap_kind");
            int kind = !cap_kind || !strcmp(cap_kind, "none") ? 0
                       : (!strcmp(cap_kind, "int") ? 1 : 2);
            out = w19_streamcons_39b4a2_raw_message_limit(
                flag(in, "is_base", 0), has_num(in, "max_message_length"),
                num(in, "max_message_length", 4096.0), flag(in, "chat_limit_raises", 0),
                has_num(in, "chat_limit"), num(in, "chat_limit", 0.0), kind,
                num(in, "cap", 0.0));
        } else if (!strcmp(func, "open_code_fence_span")) {
            out = w19_streamcons_39b4a2_open_code_fence_span(opt_s(in, "text"),
                                                             (long)num(in, "upto", 0.0));
        } else if (!strcmp(func, "ensure_closed_code_fences")) {
            out = w19_streamcons_39b4a2_ensure_closed_code_fences(opt_s(in, "text"));
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
