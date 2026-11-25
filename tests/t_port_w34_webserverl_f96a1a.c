/* JSON-lines harness for the w34_webserverl_f96a1a gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w34_webserverl_f96a1a.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
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
        if (!strcmp(func, "constants")) {
            out = w34_webserverl_f96a1a_constants();
        } else if (!strcmp(func, "valid_marker")) {
            out = w34_webserverl_f96a1a_valid_parent_start_marker(in);
        } else if (!strcmp(func, "markers_match")) {
            out = w34_webserverl_f96a1a_parent_start_markers_match(in);
        } else if (!strcmp(func, "mismatch_conclusive")) {
            out = w34_webserverl_f96a1a_parent_start_marker_mismatch_is_conclusive(in);
        } else if (!strcmp(func, "orphaned")) {
            out = w34_webserverl_f96a1a_serve_orphaned_decision(in);
        } else if (!strcmp(func, "watchdog_plan")) {
            out = w34_webserverl_f96a1a_watchdog_plan(in);
        } else if (!strcmp(func, "addr_in_use")) {
            out = w34_webserverl_f96a1a_addr_in_use(in);
        } else if (!strcmp(func, "port_preflight")) {
            out = w34_webserverl_f96a1a_port_preflight(in);
        } else if (!strcmp(func, "sentinel")) {
            out = w34_webserverl_f96a1a_port_in_use_sentinel(in);
        } else if (!strcmp(func, "hint")) {
            out = w34_webserverl_f96a1a_port_in_use_hint(in);
        } else if (!strcmp(func, "allow_ips")) {
            out = w34_webserverl_f96a1a_forwarded_allow_ips(in);
        } else if (!strcmp(func, "browser")) {
            out = w34_webserverl_f96a1a_browser_open_url(in);
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
