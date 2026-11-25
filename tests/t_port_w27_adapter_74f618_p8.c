/* JSON-lines harness for the w27_adapter_74f618_p8 gate. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_74f618_p8.h"

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
        if (!strcmp(func, "format_message")) out = w27_p8_format_message(in);
        else if (!strcmp(func, "append_block_text")) out = w27_p8_append_block_text(in);
        else if (!strcmp(func, "normalize_changed_message")) out = w27_p8_normalize_changed_message(in);
        else if (!strcmp(func, "append_link_unfurls")) out = w27_p8_append_link_unfurls(in);
        else if (!strcmp(func, "media_message_type")) out = w27_p8_media_message_type(in);
        else if (!strcmp(func, "session_thread_ts")) out = w27_p8_session_thread_ts(in);
        else if (!strcmp(func, "button")) out = w27_p8_button(in);
        else if (!strcmp(func, "interaction_fields")) out = w27_p8_interaction_fields(in);
        else if (!strcmp(func, "section_text")) out = w27_p8_section_text(in);
        else if (!strcmp(func, "thread_cache_key")) out = w27_p8_thread_cache_key(in);
        else if (!strcmp(func, "thread_root_message")) out = w27_p8_thread_root_message(in);
        else if (!strcmp(func, "render_message_text")) out = w27_p8_render_message_text(in);
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
