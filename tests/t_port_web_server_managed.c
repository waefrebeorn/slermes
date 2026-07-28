/*
 * t_port_web_server_managed.c — behavioral oracle harness for the dashboard
 * managed-files security cluster (port_web_server_managed_files.c).
 *
 * Reads a fixture JSON from argv[1]:
 *   {"op":"sensitive_filename","name":"..."}      -> {"sensitive":true|false}
 *   {"op":"sensitive_path","path":"..."}          -> {"sensitive":true|false}
 *   {"op":"chat_image_ext","b64":"..."}           -> {"ext":".png"|null,"allowed":bool}
 *   {"op":"sanitize_filename","filename":"..."}   -> {"name":"..."}
 * Emits a single JSON object on stdout for diff vs the live-Python oracle.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "web_server_managed_files.h"
#include "hermes_json.h"
#include "base64.h"

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s fixture.json\n", argv[0]); return 2; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "no fixture\n"); return 2; }
    char buf[65536];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);

    json_t *fx = json_parse(buf, NULL);
    if (!fx) { fprintf(stderr, "bad fixture json\n"); return 2; }
    const char *op = json_get_str(fx, "op", "");

    if (strcmp(op, "sensitive_filename") == 0) {
        const char *name = json_get_str(fx, "name", "");
        printf("{\"sensitive\":%s}\n", ws_is_sensitive_filename(name) ? "true" : "false");
    } else if (strcmp(op, "sensitive_path") == 0) {
        const char *path = json_get_str(fx, "path", "");
        printf("{\"sensitive\":%s}\n", ws_is_sensitive_path(path) ? "true" : "false");
    } else if (strcmp(op, "chat_image_ext") == 0) {
        const char *b64 = json_get_str(fx, "b64", "");
        size_t dlen = 0;
        unsigned char *data = base64_decode(b64, &dlen);
        const char *ext = ws_chat_image_extension(data ? data : (const unsigned char *)"", dlen);
        if (ext)
            printf("{\"ext\":\"%s\",\"allowed\":%s}\n", ext,
                   ws_chat_image_extension_allowed(ext) ? "true" : "false");
        else
            printf("{\"ext\":null,\"allowed\":false}\n");
        free(data);
    } else if (strcmp(op, "sanitize_filename") == 0) {
        const char *fn = json_get_str(fx, "filename", "");
        char out[512];
        ws_sanitize_chat_image_filename(fn, out, sizeof(out));
        printf("{\"name\":\"%s\"}\n", out);
    } else {
        fprintf(stderr, "unknown op %s\n", op);
        return 2;
    }
    json_free(fx);
    return 0;
}
