/* Port gate harness for hermes_cli/cli_panel.py (gate clipanel). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_clipanel.h"

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
        if (!strcmp(func, "panel")) {
            clipanel_panel_t p;
            clipanel_init(&p, json_get_str(in, "border", "B"),
                          (int)json_get_num(in, "width", 0),
                          json_get_str(in, "title", ""),
                          json_get_str(in, "title_style", ""));
            json_t *ops = json_obj_get(in, "ops");
            size_t nops = json_is_array(ops) ? json_len(ops) : 0;
            for (size_t i = 0; i < nops; i++) {
                json_t *o = json_get(ops, i);
                const char *op = json_get_str(o, "op", "");
                if (!strcmp(op, "row"))
                    clipanel_row(&p, json_get_str(o, "style", ""), json_get_str(o, "text", ""));
                else if (!strcmp(op, "blank"))
                    clipanel_blank(&p);
                else if (!strcmp(op, "close"))
                    clipanel_close(&p);
            }
            out = json_array();
            for (size_t i = 0; i < p.count; i++) {
                json_t *pair = json_array();
                json_append(pair, json_string(p.lines[i].style));
                json_append(pair, json_string(p.lines[i].text));
                json_append(out, pair);
            }
            clipanel_free(&p);
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
