/* t_port_w14_chatcomple_840065.c — JSON-lines harness for chat_completion_local.c */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w14_chatcomple_840065.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static const char *str_arg(const json_t *in, const char *key) {
    const json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static bool bool_arg(const json_t *in, const char *key) {
    const json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_BOOL) ? v->bool_val : false;
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "stale_giveup")) {
            out = json_number((double)w14_chatcomple_840065_stale_giveup_ex(str_arg(in, "env_value")));
        } else if (!strcmp(func, "streams_llamacpp")) {
            out = json_bool(w14_chatcomple_840065_streams_llamacpp_ex(
                str_arg(in, "api_mode"), str_arg(in, "provider"), str_arg(in, "base_url"),
                bool_arg(in, "managed")));
        } else if (!strcmp(func, "retry_recovered_local_server")) {
            out = w14_chatcomple_840065_retry_recovered_local_server_ex(
                (long long)json_get_num(in, "giveup", 0), (long long)json_get_num(in, "streak", 0),
                bool_arg(in, "streams"), bool_arg(in, "answers"));
        } else {
            out = json_null();
        }
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); ++i) {
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
