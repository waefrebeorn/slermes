/* t_port_w13_browserrun_9220bd.c — oracle harness for
 * hermes_cli/browser_runtime.py (gate w13_browserrun_9220bd). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_browserrun_9220bd.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

/* NULL from the C port is Python None -> JSON null. */
static json_t *opt_str(char *s) {
    json_t *v = s ? json_string(s) : json_null();
    free(s);
    return v;
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

        if (!strcmp(func, "chromium_executable")) {
            char *r = w13_browserrun_9220bd_chromium_executable(
                json_get_bool(in, "allow_override", true) ? 1 : 0,
                json_get_str(in, "configured", ""),
                json_get_str(in, "pm_exec", ""),
                json_get_str(in, "baked", ""),
                json_get_str(in, "system", ""),
                json_get_bool(in, "pm_no", false) ? 1 : 0);
            out = opt_str(r);
        } else if (!strcmp(func, "chromium_missing_hint")) {
            char *r = w13_browserrun_9220bd_chromium_missing_hint(
                json_get_str(in, "pinned", ""),
                json_get_bool(in, "termux", false) ? 1 : 0,
                json_get_bool(in, "baked_exists", false) ? 1 : 0,
                json_get_bool(in, "pm_no", false) ? 1 : 0);
            out = opt_str(r);
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
