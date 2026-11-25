#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w17_inboundrep_dae3a2.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static int getbool(json_t *in, const char *k) {
    json_t *v = json_obj_get(in, k);
    return v && v->type == JSON_BOOL && v->bool_val ? 1 : 0;
}

int main(void) {
    char line[1 << 20];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "chat_key")) {
            char *s = w17_inboundrep_dae3a2_chat_key(
                json_obj_get(in, "platform"), json_obj_get(in, "chat_id"));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "field")) {
            out = w17_inboundrep_dae3a2_field(json_obj_get(in, "result"),
                                              json_get_str(in, "key", ""));
        } else if (!strcmp(func, "pointer")) {
            const char *txt = json_get_str(in, "text", "");
            const char *san = json_get_str(in, "sanitized", txt);
            char *s = w17_inboundrep_dae3a2_reply_pointer(
                san, getbool(in, "own_message"), getbool(in, "has_message_id"),
                json_get_str(in, "header", NULL));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "record")) {
            out = w17_inboundrep_dae3a2_record_cron_delivery(
                json_obj_get(in, "platform"), json_obj_get(in, "chat_id"),
                json_get_str(in, "text", ""), json_obj_get(in, "send_result"));
            if (!out) out = json_null();
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
