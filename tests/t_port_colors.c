#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_colors.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

/* NO_COLOR / TERM are injected explicitly: on Windows an empty env var cannot
 * be represented (setenv("",...) unsets), but Python's os.environ keeps it. */

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;

        json_t *ncv = json_obj_get(in, "no_color");
        json_t *tv = json_obj_get(in, "term");
        const char *no_color = (ncv && ncv->type == JSON_STRING) ? ncv->str_val : NULL;
        const char *term = (tv && tv->type == JSON_STRING) ? tv->str_val : NULL;
        int tty = json_get_bool(in, "tty", 0) ? 1 : 0;

        if (!strcmp(func, "should_use_color")) {
            out = json_bool(colors_should_use_color_ex(tty, no_color, term) ? true : false);
        } else if (!strcmp(func, "color")) {
            const char *text = json_get_str(in, "text", "");
            json_t *codes = json_obj_get(in, "codes");
            char **arr = NULL;
            size_t n = 0;
            if (codes && codes->type == JSON_ARRAY) {
                n = codes->c.count;
                arr = (char **)calloc(n ? n : 1, sizeof(char *));
                for (size_t i = 0; i < n; i++) {
                    json_t *c = codes->c.items[i];
                    arr[i] = (c && c->type == JSON_STRING) ? c->str_val : (char *)"";
                }
            }
            char *s = colors_color_ex(tty, text, arr, n, no_color, term);
            out = json_string(s ? s : "");
            free(s);
            free(arr);
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
