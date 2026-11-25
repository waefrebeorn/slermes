/* JSON-lines harness for the w27_adapter_878564_p5 gate (photon adapter: mention
 * and presence gates).  One request object per stdin line; one response line
 * carrying "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_878564_p5.h"

static void emit_val(json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static void emit_key(const char *key, json_t *v) {
    printf("\"%s\":", key);
    emit_val(v);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "mention_pattern_texts")) {
            out = w27_adapter_878564_p5_mention_pattern_texts(json_obj_get(in, "raw"));
        } else if (!strcmp(func, "mention_match")) {
            out = json_bool(w27_adapter_878564_p5_mention_match(
                                json_get_str(in, "text", ""),
                                json_obj_get(in, "patterns")) != 0);
        } else if (!strcmp(func, "clean_mention_text")) {
            char *s = w27_adapter_878564_p5_clean_mention_text(
                json_get_str(in, "text", ""), json_obj_get(in, "patterns"));
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "gate_stt_chat")) {
            out = w27_adapter_878564_p5_gate_stt_chat(
                json_get_str(in, "space_id", ""), json_get_str(in, "space_type", NULL),
                json_obj_get(in, "content"), json_get_bool(in, "require_mention", false),
                json_get_bool(in, "voice_gate_enabled", false));
        } else if (!strcmp(func, "require_mention_flag")) {
            out = json_bool(w27_adapter_878564_p5_require_mention_flag(
                                json_obj_get(in, "value"), json_obj_get(in, "env")) != 0);
        } else if (!strcmp(func, "markdown_enabled")) {
            out = json_bool(w27_adapter_878564_p5_markdown_enabled(json_obj_get(in, "value")) != 0);
        } else if (!strcmp(func, "reactions_enabled")) {
            out = json_bool(w27_adapter_878564_p5_reactions_enabled(json_obj_get(in, "value")) != 0);
        } else if (!strcmp(func, "autostart_enabled")) {
            out = json_bool(w27_adapter_878564_p5_autostart_enabled(json_obj_get(in, "value")) != 0);
        } else if (!strcmp(func, "is_recent_richlink_preview")) {
            out = w27_adapter_878564_p5_is_recent_richlink_preview(
                json_get_str(in, "chat_id", ""), json_obj_get(in, "content"),
                json_obj_get(in, "last"), json_get_num(in, "now", 0));
        } else if (!strcmp(func, "record_recent_richlink")) {
            out = w27_adapter_878564_p5_record_recent_richlink(
                json_get_str(in, "chat_id", ""), json_get_str(in, "text", ""));
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        printf("{");
        emit_key("func", json_string(func));
        fputs(",\"out\":", stdout);
        emit_val(out);
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
