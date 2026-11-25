/* Port gate harness for tools/approval_smart.py (gate w7_approvalsma_cffb5c). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_approvalsma_cffb5c.h"

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
        const char *text = json_get_str(in, "text", "");
        const char *command = json_get_str(in, "command", "");
        const char *answer = json_get_str(in, "answer", "");
        long start = (long)json_get_num(in, "start", 0);
        json_t *out = NULL;
        if (!strcmp(func, "next_comment")) {
            out = apprsmart_next_comment(text, start);
        } else if (!strcmp(func, "comment_spans")) {
            out = apprsmart_comment_spans(text);
        } else if (!strcmp(func, "strip_line_comment")) {
            out = apprsmart_strip_line_comment(text);
        } else if (!strcmp(func, "strip_shell_comments")) {
            out = apprsmart_strip_shell_comments(command);
        } else if (!strcmp(func, "has_heredoc")) {
            out = apprsmart_has_heredoc(command);
        } else if (!strcmp(func, "verdict")) {
            out = apprsmart_verdict(answer);
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
