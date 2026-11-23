/* Port gate harness for agent/proxy_bypass.py (gate w6_proxybypass_aae960). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_proxybypass_aae960.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "split")) {
            out = proxybypass_split_host_port(json_get_str(in, "value", ""));
        } else if (!strcmp(func, "entries")) {
            out = proxybypass_no_proxy_entries(json_get_str(in, "value", ""));
        } else if (!strcmp(func, "loopback")) {
            out = proxybypass_is_loopback(json_get_str(in, "host", ""));
        } else if (!strcmp(func, "match")) {
            out = proxybypass_entry_matches(
                json_get_str(in, "entry", ""), json_get_str(in, "host", ""),
                json_get_bool(in, "has_port", false) ? 1 : 0,
                json_get_num(in, "port", 0));
        } else if (!strcmp(func, "bypass")) {
            out = proxybypass_should_bypass(
                json_obj_get(in, "targets"), json_get_str(in, "no_proxy_value", ""));
        } else if (!strcmp(func, "connect")) {
            out = proxybypass_connect_kwargs(json_get_str(in, "url", ""));
        } else if (!strcmp(func, "request")) {
            out = proxybypass_request_kwargs(json_get_str(in, "url", ""));
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
