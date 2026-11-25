/* t_port_w5_urls_e1571a.c — differential harness for utils/urls.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_urls_e1571a.h"

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
        const char *url = json_get_str(in, "url", "");
        json_t *out = NULL;

        if (!strcmp(func, "hostname")) {
            char b[2048];
            w5urls_hostname(url, b, sizeof(b));
            out = json_string(b);
        } else if (!strcmp(func, "path")) {
            char b[8192];
            w5urls_path(url, b, sizeof(b));
            out = json_string(b);
        } else if (!strcmp(func, "origin")) {
            char sc[128], ho[2048];
            int port = 0;
            json_t *arr = json_array();
            if (w5urls_origin(url, sc, sizeof(sc), ho, sizeof(ho), &port)) {
                json_append(arr, json_string(sc));
                json_append(arr, json_string(ho));
                json_append(arr, json_number((double)port));
            } else {
                json_append(arr, json_string(""));
                json_append(arr, json_string(""));
                json_append(arr, json_number(0));
            }
            out = arr;
        } else if (!strcmp(func, "same")) {
            out = json_bool(w5urls_same_origin(url,
                          json_get_str(in, "b", "")) ? 1 : 0);
        } else if (!strcmp(func, "match")) {
            out = json_bool(w5urls_host_matches(url,
                           json_get_str(in, "domain", "")) ? 1 : 0);
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
