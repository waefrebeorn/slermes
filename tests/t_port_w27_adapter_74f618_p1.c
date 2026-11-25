/* JSON-lines harness for the w27_adapter_74f618_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_74f618_p1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[1048576];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "unfurl_kwargs")) {
            out = w27_p1_unfurl_kwargs(in);
        } else if (!strcmp(func, "is_table_row")) {
            out = w27_p1_is_table_row(in);
        } else if (!strcmp(func, "disp_width")) {
            out = w27_p1_disp_width(in);
        } else if (!strcmp(func, "pad")) {
            out = w27_p1_pad(in);
        } else if (!strcmp(func, "split_table_row")) {
            out = w27_p1_split_table_row(in);
        } else if (!strcmp(func, "align_table")) {
            out = w27_p1_align_table(in);
        } else if (!strcmp(func, "wrap_markdown_tables")) {
            out = w27_p1_wrap_markdown_tables(in);
        } else if (!strcmp(func, "permalink_path")) {
            out = w27_p1_permalink_path(in);
        } else if (!strcmp(func, "str_or_empty")) {
            out = w27_p1_str_or_empty(in);
        } else if (!strcmp(func, "int_or_zero")) {
            out = w27_p1_int_or_zero(in);
        } else if (!strcmp(func, "first_truthy")) {
            out = w27_p1_first_truthy(in);
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
