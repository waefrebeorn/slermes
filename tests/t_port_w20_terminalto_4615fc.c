/* JSON-lines harness for the w20_terminalto_4615fc gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_terminalto_4615fc.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

/* A string field, or NULL when absent/null/not a string (Python None). */
static const char *opt_str(json_t *in, const char *key) {
    json_t *v = json_obj_get(in, key);
    if (!v || v->type != JSON_STRING) return NULL;
    return v->str_val;
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "transform")) {
            char *s = w20_terminalto_4615fc_transform(opt_str(in, "command"),
                                                     opt_str(in, "system"));
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "ladder")) {
            char *s = w20_terminalto_4615fc_ladder(opt_str(in, "app"), opt_str(in, "file"));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "quote")) {
            char *s = w20_terminalto_4615fc_quote(opt_str(in, "value"));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "osascript_app")) {
            char *s = w20_terminalto_4615fc_osascript_app(opt_str(in, "script"));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "flags")) {
            out = w20_terminalto_4615fc_no_transform_flags();
        } else if (!strcmp(func, "split")) {
            out = w20_terminalto_4615fc_split(opt_str(in, "command"));
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
