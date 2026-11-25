/* t_port_w18_webserver_e8a789.c — harness for src/hermes_cli/web_server_boundary.c */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w18_webserver_e8a789.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "public_url_hosts")) {
            out = w18_webserver_e8a789_public_url_hosts(json_get_str(in, "url", ""));
        } else if (!strcmp(func, "host_header")) {
            out = json_string(w18_webserver_e8a789_host_header_hostname(json_get_str(in, "header", "")));
        } else if (!strcmp(func, "is_accepted")) {
            out = json_bool(w18_webserver_e8a789_is_accepted_host(
                        json_get_str(in, "header", ""), json_get_str(in, "bound", ""),
                        json_obj_get(in, "trusted")) ? true : false);
        } else if (!strcmp(func, "should_auth")) {
            out = json_bool(w18_webserver_e8a789_should_require_auth(
                        json_get_str(in, "host", ""),
                        json_get_bool(in, "allow_public", false) ? 1 : 0) ? true : false);
        } else if (!strcmp(func, "engage_gate")) {
            out = json_bool(w18_webserver_e8a789_public_hosts_engage_gate(
                        json_obj_get(in, "trusted")) ? true : false);
        } else if (!strcmp(func, "loopback_origin")) {
            out = w18_webserver_e8a789_loopback_origin(json_get_str(in, "url", ""));
        } else if (!strcmp(func, "public_url_origins")) {
            out = w18_webserver_e8a789_public_url_origins(json_get_str(in, "url", ""));
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        printf("{");
        json_t *jf = json_string(func);
        emit_key("func", jf);
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
        json_free(out); json_free(jf); json_free(in);
    }
    return 0;
}
