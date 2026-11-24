/* JSON-lines harness for the w27_adapter_878564_p3 gate (photon adapter:
 * normalize ladder + outbound payload bodies).  One request object per stdin
 * line; one response line carrying the computed "out" plus every input key
 * echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_878564_p3.h"
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
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "normalize_binary_payload")) {
            out = w27_adapter_878564_p3_normalize_binary_payload(
                json_obj_get(in, "payload"), json_get_str(in, "cached", NULL));
        } else if (!strcmp(func, "normalize_group_content")) {
            out = w27_adapter_878564_p3_normalize_group_content(
                json_obj_get(in, "content"), json_obj_get(in, "cache"));
        } else if (!strcmp(func, "normalize_content")) {
            out = w27_adapter_878564_p3_normalize_content(
                json_obj_get(in, "content"), json_obj_get(in, "cache"));
        } else if (!strcmp(func, "attachment_body")) {
            out = w27_adapter_878564_p3_attachment_body(
                json_get_str(in, "space_id", ""), json_get_str(in, "safe_path", ""),
                json_get_str(in, "kind", NULL), json_get_str(in, "name", NULL),
                json_get_str(in, "mime_type", NULL), json_get_str(in, "caption", NULL));
        } else if (!strcmp(func, "attachment_label")) {
            char *s = w27_adapter_878564_p3_attachment_label(json_get_str(in, "kind", ""),
                                                             json_get_str(in, "name", ""));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "send_plan")) {
            bool md = w27_adapter_878564_p5_markdown_enabled(json_obj_get(in, "markdown_env")) != 0;
            out = w27_adapter_878564_p3_send_plan(
                json_get_str(in, "space_id", ""), json_get_str(in, "text", ""),
                json_get_bool(in, "richlink", false), json_get_bool(in, "markdown", true), md);
        } else if (!strcmp(func, "send_poll_plan")) {
            out = w27_adapter_878564_p3_send_poll_plan(
                json_get_str(in, "space_id", ""), json_get_str(in, "title", NULL),
                json_obj_get(in, "options"));
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
        json_free(out);
        json_free(in);
    }
    return 0;
}
