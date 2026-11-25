/* JSON-lines harness for the w27_adapter_878564_p1 gate (photon adapter: text,
 * markdown, urls).  One request object per stdin line; one response line carrying
 * the computed "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_878564_p1.h"
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

static void emit_str(const char *s) {
    if (s) {
        char *e = json_escape_string(s);
        fputs(e ? e : "null", stdout);
        free(e);
    } else {
        fputs("null", stdout);
    }
}

static const char *opt_str(const json_t *in, const char *k) {
    return json_get_str(in, k, NULL);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in;
        const char *func;
        char *out = NULL;
        json_t *outj = NULL;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "strip_markdown")) {
            out = w27_adapter_878564_p1_strip_markdown(opt_str(in, "text"),
                                                       json_get_bool(in, "keep_link_targets", false));
        } else if (!strcmp(func, "format_message")) {
            bool md = w27_adapter_878564_p5_markdown_enabled(json_obj_get(in, "markdown_env")) != 0;
            out = w27_adapter_878564_p1_format_message(opt_str(in, "content"), md);
        } else if (!strcmp(func, "sidecar_payload_text")) {
            out = w27_adapter_878564_p1_sidecar_payload_text(
                opt_str(in, "text"), json_get_bool(in, "send_markdown", false));
        } else if (!strcmp(func, "url_only_candidate")) {
            out = w27_adapter_878564_p1_url_only_candidate(opt_str(in, "text"));
        } else if (!strcmp(func, "richlink_candidate")) {
            bool md = w27_adapter_878564_p5_markdown_enabled(json_obj_get(in, "markdown_env")) != 0;
            out = w27_adapter_878564_p1_richlink_candidate(opt_str(in, "text"), md);
        } else if (!strcmp(func, "format_richlink_content")) {
            out = w27_adapter_878564_p1_format_richlink_content(json_obj_get(in, "content"));
        } else {
            outj = json_string("UNKNOWN_FUNC");
        }
        printf("{");
        emit_key("func", json_string(func));
        fputs(",\"out\":", stdout);
        if (outj) emit_val(outj);
        else emit_str(out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        fflush(stdout);
        free(out);
        json_free(outj);
        json_free(in);
    }
    return 0;
}
