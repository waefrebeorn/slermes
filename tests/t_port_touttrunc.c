#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_touttrunc.h"

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
        if (!strcmp(func, "notice")) {
            long long omitted = (long long)json_get_num(in, "omitted", 0);
            long long total = (long long)json_get_num(in, "total", 0);
            const char *label = json_get_str(in, "label", "OUTPUT");
            const char *unit = json_get_str(in, "unit", "chars");
            out = json_string(touttrunc_notice(omitted, total, label, unit));
        } else if (!strcmp(func, "split")) {
            long long budget = (long long)json_get_num(in, "budget", 0);
            long long h, t;
            touttrunc_head_tail_split(budget, &h, &t);
            out = json_array();
            json_append(out, json_number((double)h));
            json_append(out, json_number((double)t));
        } else if (!strcmp(func, "truncate")) {
            const char *text = json_get_str(in, "text", "");
            long long mx = (long long)json_get_num(in, "max_chars", 0);
            const char *label = json_get_str(in, "label", "OUTPUT");
            char *s = touttrunc_head_tail(text, mx, label);
            out = json_string(s);
            free(s);
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
