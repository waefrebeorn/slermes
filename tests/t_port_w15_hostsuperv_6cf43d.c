/* Port gate harness for tui_gateway/host_supervisor.py (gate w15_hostsuperv_6cf43d). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_hostsuperv_6cf43d.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *jstr(json_t *in, const char *k, const char *def) {
    return json_get_str(in, k, def);
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
        char *tmp = NULL;
        if (!strcmp(func, "identity")) {
            out = json_bool(w15_hostsuperv_6cf43d_is_compute_host_identity(jstr(in, "command", "")));
        } else if (!strcmp(func, "log_text")) {
            tmp = w15_hostsuperv_6cf43d_log_record_text(jstr(in, "record", ""));
            out = json_string(tmp ? tmp : "");
        } else if (!strcmp(func, "hello")) {
            out = w15_hostsuperv_6cf43d_validate_hello(
                json_get_bool(in, "hello_present", 0) ? 1 : 0,
                jstr(in, "hello_home", ""), jstr(in, "hello_sha", ""),
                jstr(in, "expected_home", ""), jstr(in, "expected_sha", ""));
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
        free(tmp);
        json_free(out);
        json_free(in);
    }
    return 0;
}
