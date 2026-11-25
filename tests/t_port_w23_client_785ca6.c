/* JSON-lines harness for the w23_client_785ca6 gate (agent/lsp/client.py).
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_client_785ca6.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) {
        fputs(s, stdout);
        free(s);
    } else {
        fputs("null", stdout);
    }
}

static json_t *out_string(char *s) {
    json_t *n = json_string(s ? s : "");
    free(s);
    return n;
}

/* The diagnostic key carries literal NUL separators, so it is printed from an
 * explicit length with the escapes a JSON encoder would emit. */
static void emit_json_string_n(const char *s, size_t n) {
    size_t i;
    putchar('"');
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '"') fputs("\\\"", stdout);
        else if (c == '\\') fputs("\\\\", stdout);
        else if (c == '\n') fputs("\\n", stdout);
        else if (c == '\r') fputs("\\r", stdout);
        else if (c == '\t') fputs("\\t", stdout);
        else if (c < 0x20) printf("\\u%04x", (unsigned)c);
        else putchar(c);
    }
    putchar('"');
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        char *raw_out = NULL;
        size_t raw_len = 0;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "file_uri")) {
            out = out_string(w23_client_785ca6_file_uri(json_get_str(in, "path", NULL),
                                                       json_get_str(in, "os_name", "nt")));
        } else if (!strcmp(func, "uri_to_path")) {
            out = out_string(w23_client_785ca6_uri_to_path(json_get_str(in, "uri", NULL),
                                                          json_get_str(in, "os_name", "nt")));
        } else if (!strcmp(func, "folder")) {
            out = w23_client_785ca6_folder(json_get_str(in, "root", NULL),
                                           json_get_str(in, "os_name", "nt"));
        } else if (!strcmp(func, "end_position")) {
            out = w23_client_785ca6_end_position(json_get_str(in, "text", NULL));
        } else if (!strcmp(func, "as_disk_read")) {
            out = out_string(w23_client_785ca6_as_disk_read(json_get_str(in, "text", NULL)));
        } else if (!strcmp(func, "diagnostic_key")) {
            raw_out = w23_client_785ca6_diagnostic_key(json_obj_get(in, "diag"), &raw_len);
            out = NULL;
        } else if (!strcmp(func, "dedupe")) {
            out = w23_client_785ca6_dedupe(json_obj_get(in, "lists"));
        } else if (!strcmp(func, "doc_fresh")) {
            out = json_bool(w23_client_785ca6_doc_fresh(json_obj_get(in, "doc"),
                                                        json_get_str(in, "kind", "any"),
                                                        json_obj_get(in, "at")) != 0);
        } else if (!strcmp(func, "diagnostics_for")) {
            out = w23_client_785ca6_diagnostics_for(in);
        } else if (!strcmp(func, "document_text")) {
            out = w23_client_785ca6_document_text(in);
        } else if (!strcmp(func, "config_section")) {
            out = w23_client_785ca6_config_section(json_obj_get(in, "item"), json_obj_get(in, "init"));
        } else if (!strcmp(func, "workspace_configuration")) {
            out = w23_client_785ca6_workspace_configuration(json_obj_get(in, "params"),
                                                            json_obj_get(in, "init"));
        } else if (!strcmp(func, "limits")) {
            out = w23_client_785ca6_limits();
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        if (raw_out) {
            fputs("\"out\":", stdout);
            emit_json_string_n(raw_out, raw_len);
            free(raw_out);
        } else {
            emit_key("out", out);
        }
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
