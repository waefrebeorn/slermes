/* JSON-lines harness for the w28_hostedroom_3fa737_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w28_hostedroom_3fa737_p1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *jstr(const json_t *in, const char *key, const char *def) {
    const json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : def;
}

static json_t *jval(const json_t *in, const char *key) { return json_obj_get(in, key); }

static double jnum(const json_t *in, const char *key, double def) {
    const json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_NUMBER) ? v->num_val : def;
}

static long long jll(const json_t *in, const char *key, long long def) {
    const json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_NUMBER) ? (long long)v->num_val : def;
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = jstr(in, "func", "");
        if (!strcmp(func, "room_session_title")) {
            out = w28_hostedroom_3fa737_p1_room_session_title(jstr(in, "room_id", ""));
        } else if (!strcmp(func, "member_id")) {
            out = w28_hostedroom_3fa737_p1_member_id(jval(in, "task"));
        } else if (!strcmp(func, "session_id")) {
            out = w28_hostedroom_3fa737_p1_session_id(jval(in, "session"));
        } else if (!strcmp(func, "truncate_utf8")) {
            out = w28_hostedroom_3fa737_p1_truncate_utf8(jstr(in, "text", ""),
                                                         jll(in, "max_bytes", 0));
        } else if (!strcmp(func, "bounded_result")) {
            out = w28_hostedroom_3fa737_p1_bounded_result(jval(in, "receipt"));
        } else if (!strcmp(func, "receipt_from")) {
            out = w28_hostedroom_3fa737_p1_receipt_from(jval(in, "receipt"),
                                                        jstr(in, "task_id", ""),
                                                        jll(in, "generation", 0));
        } else if (!strcmp(func, "discarded_receipt")) {
            out = w28_hostedroom_3fa737_p1_discarded_receipt(jval(in, "result"), jval(in, "task"));
        } else if (!strcmp(func, "find_terminal_receipt")) {
            out = w28_hostedroom_3fa737_p1_find_terminal_receipt(jval(in, "history"),
                                                                 jstr(in, "task_id", ""),
                                                                 jll(in, "generation", 0));
        } else if (!strcmp(func, "session_kw")) {
            out = w28_hostedroom_3fa737_p1_session_kw(jstr(in, "profile", ""),
                                                      jstr(in, "session_id", ""));
        } else if (!strcmp(func, "fences")) {
            out = w28_hostedroom_3fa737_p1_fences(jval(in, "task"));
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
        (void)jnum;
    }
    return 0;
}
