/* JSON-lines harness for the w27_adapter_74f618_p2 gate. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_74f618_p2.h"

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
        if (!strcmp(func, "collect_block_mentions")) out = w27_p2_collect_block_mentions(in);
        else if (!strcmp(func, "mention_detection_text")) out = w27_p2_mention_detection_text(in);
        else if (!strcmp(func, "slack_str_field")) out = w27_p2_slack_str_field(in);
        else if (!strcmp(func, "render_inline_element")) out = w27_p2_render_inline_element(in);
        else if (!strcmp(func, "render_inline_elements")) out = w27_p2_render_inline_elements(in);
        else if (!strcmp(func, "extract_text_blocks")) out = w27_p2_extract_text_blocks(in);
        else if (!strcmp(func, "table_cell_text")) out = w27_p2_table_cell_text(in);
        else if (!strcmp(func, "render_table_block")) out = w27_p2_render_table_block(in);
        else if (!strcmp(func, "slack_error_is")) out = w27_p2_slack_error_is(in);
        else out = json_string("UNKNOWN_FUNC");
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
