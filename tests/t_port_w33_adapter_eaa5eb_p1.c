/* JSON-lines harness for the w33_adapter_eaa5eb_p1 gate (IRC adapter,
 * wire format). One request object per stdin line; one response line carrying
 * the computed "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w33_adapter_eaa5eb_p1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *dispatch(const char *func, json_t *in) {
    if (!strcmp(func, "parse_message"))
        return w33_adapter_eaa5eb_p1_parse_message(in);
    if (!strcmp(func, "extract_nick"))
        return w33_adapter_eaa5eb_p1_extract_nick(in);
    if (!strcmp(func, "strip_markdown"))
        return w33_adapter_eaa5eb_p1_strip_markdown(in);
    if (!strcmp(func, "chunk_paragraph"))
        return w33_adapter_eaa5eb_p1_chunk_paragraph(in);
    if (!strcmp(func, "split_lines"))
        return w33_adapter_eaa5eb_p1_split_lines(in);
    if (!strcmp(func, "privmsg_budget"))
        return w33_adapter_eaa5eb_p1_privmsg_budget(in);
    if (!strcmp(func, "split_message"))
        return w33_adapter_eaa5eb_p1_split_message(in);
    if (!strcmp(func, "encode_line"))
        return w33_adapter_eaa5eb_p1_encode_line(in);
    if (!strcmp(func, "sanitize_outbound"))
        return w33_adapter_eaa5eb_p1_sanitize_outbound(in);
    if (!strcmp(func, "ping_reply"))
        return w33_adapter_eaa5eb_p1_ping_reply(in);
    return json_string("UNKNOWN_FUNC");
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        out = dispatch(func, in);
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
