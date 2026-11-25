#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_codeexecut_b29647.h"

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
        if (!strcmp(func, "private_dirs_cmd")) {
            out = w11_codeexecut_b29647_private_dirs_cmd(json_get_str(in, "root", ""),
                                                         json_obj_get(in, "subdirs"));
        } else if (!strcmp(func, "rpc_token_ok")) {
            out = w11_codeexecut_b29647_rpc_token_ok(json_obj_get(in, "request"),
                        json_get_str(in, "rpc_token", ""));
        } else if (!strcmp(func, "result_error")) {
            out = w11_codeexecut_b29647_result_error(json_get_str(in, "result", ""));
        } else if (!strcmp(func, "tool_errors_since")) {
            /* int64 arrives as a decimal STRING (json numbers are double). */
            long long start = strtoll(json_get_str(in, "start", "0"), NULL, 10);
            out = w11_codeexecut_b29647_tool_errors_since(json_obj_get(in, "tool_call_log"), start);
        } else if (!strcmp(func, "execute_checked")) {
            long long to = strtoll(json_get_str(in, "timeout", "0"), NULL, 10);
            out = w11_codeexecut_b29647_execute_checked(json_get_str(in, "cmd", ""),
                    json_get_str(in, "what", ""), to, json_obj_get(in, "execute_result"));
        } else if (!strcmp(func, "remote_write")) {
            long long to = strtoll(json_get_str(in, "timeout", "30"), NULL, 10);
            out = w11_codeexecut_b29647_remote_write(
                    json_get_str(in, "remote_path", ""), json_get_str(in, "content", ""),
                    json_get_bool(in, "atomic", false) ? 1 : 0, to,
                    json_get_bool(in, "check", false) ? 1 : 0,
                    json_obj_get(in, "execute_result"));
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
