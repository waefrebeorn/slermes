#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_mcptooldi_fd99f7.h"

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
        const json_t *servers = json_obj_get(in, "servers");
        const json_t *errs = json_obj_get(in, "connect_errors");
        const json_t *connecting = json_obj_get(in, "connecting");
        const json_t *lazy = json_obj_get(in, "lazy_configs");
        const json_t *retry = json_obj_get(in, "retry_after");
        double now = json_get_num(in, "now", 0.0);
        if (!strcmp(func, "connected_summary")) {
            long long lt = strtoll(json_get_str(in, "lazy_tools", "0"), NULL, 10);
            long long ls = strtoll(json_get_str(in, "lazy_servers", "0"), NULL, 10);
            out = w11_mcptooldi_fd99f7_connected_summary(json_obj_get(in, "names"),
                    servers, errs, lt, ls);
        } else if (!strcmp(func, "unregistered")) {
            out = w11_mcptooldi_fd99f7_unregistered(json_obj_get(in, "wanted"),
                    servers, connecting, lazy);
        } else if (!strcmp(func, "awaiting_connect")) {
            out = w11_mcptooldi_fd99f7_awaiting_connect(json_obj_get(in, "wanted"),
                    servers, connecting, lazy, retry, now);
        } else if (!strcmp(func, "not_connected")) {
            out = w11_mcptooldi_fd99f7_not_connected(json_obj_get(in, "wanted"),
                    servers, errs, connecting, lazy, retry, now);
        } else if (!strcmp(func, "parallel_safe")) {
            out = json_bool(w11_mcptooldi_fd99f7_parallel_safe(json_get_str(in, "tool_name", ""),
                    json_obj_get(in, "mcp_tool_server_names"), json_obj_get(in, "parallel_safe")) ? true : false);
        } else if (!strcmp(func, "resolve_server_lazy")) {
            out = json_bool(w11_mcptooldi_fd99f7_resolve_server_lazy(json_obj_get(in, "config")) ? true : false);
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
