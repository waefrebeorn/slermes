#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_procwait.h"

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

        /* Per-case TERMINAL_TIMEOUT, mirroring the oracle's os.environ setup. */
        json_t *tt = json_obj_get(in, "terminal_timeout");
        if (tt && tt->type == JSON_STRING) setenv("TERMINAL_TIMEOUT", tt->str_val, 1);
        else if (tt && tt->type == JSON_NUMBER) {
            char b[64];
            snprintf(b, sizeof(b), "%.0f", tt->num_val);
            setenv("TERMINAL_TIMEOUT", b, 1);
        } else unsetenv("TERMINAL_TIMEOUT");

        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "max_wait_timeout")) {
            out = json_number((double)procwait_max_wait_timeout());
        } else if (!strcmp(func, "process_deadline_floor")) {
            json_t *args = json_obj_get(in, "args");
            const char *action = args ? json_get_str(args, "action", "") : "";
            json_t *tv = args ? json_obj_get(args, "timeout") : NULL;
            int isnull = (!tv || tv->type == JSON_NULL);
            char tbuf[512];
            const char *tstr = "";
            if (!isnull) {
                if (tv->type == JSON_STRING) tstr = tv->str_val;
                else if (tv->type == JSON_NUMBER) {
                    snprintf(tbuf, sizeof(tbuf), "%.17g", tv->num_val);
                    tstr = tbuf;
                } else if (tv->type == JSON_BOOL) tstr = tv->bool_val ? "1" : "0";
                else tstr = "[]"; /* float(list/dict) -> TypeError -> max timeout */
            }
            double val = 0;
            if (procwait_process_deadline_floor(action, tstr, isnull, &val))
                out = json_number(val);
            else
                out = json_null();
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
