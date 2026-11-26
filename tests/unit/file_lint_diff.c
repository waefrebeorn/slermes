/* file_lint_diff.c — drive the real in-process linters over stdin cases.
 *
 * Input: lines "FORMAT<TAB>base64(content)". Output: "OUT<TAB>FORMAT<TAB><the tool's own JSON verdict>".
 * Base64 keeps newlines out of the line protocol; the C side decodes.
 */
#include "file_lint.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t b64decode(const char *in, char *out, size_t cap) {
    static signed char map[256];
    static int init = 0;
    if (!init) {
        for (int i = 0; i < 256; i++) map[i] = -1;
        const char *al = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (int i = 0; i < 64; i++) map[(unsigned char)al[i]] = (signed char)i;
        init = 1;
    }
    size_t o = 0;
    int bits = 0, acc = 0;
    for (const char *p = in; *p && *p != '='; p++) {
        signed char v = map[(unsigned char)*p];
        if (v < 0) continue;
        acc = (acc << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (o + 1 < cap) out[o++] = (char)((acc >> bits) & 0xFF);
        }
    }
    out[o] = '\0';
    return o;
}

int main(void) {
    char line[16384];
    static char content[16384];
    while (fgets(line, sizeof(line), stdin)) {
        size_t len = strlen(line);
        while (len && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';
        if (!len) continue;
        char *tab = strchr(line, '\t');
        if (!tab) continue;
        *tab = '\0';
        b64decode(tab + 1, content, sizeof(content));

        char *out = NULL;
        if (strcmp(line, "json") == 0) out = file_lint_json(content);
        else if (strcmp(line, "yaml") == 0) out = file_lint_yaml(content);
        else if (strcmp(line, "toml") == 0) out = file_lint_toml(content);
        else if (strcmp(line, "python") == 0) out = file_lint_python(NULL, content);
        printf("OUT\t%s\t%s\n", line, out ? out : "NULL");
        free(out);
    }
    return 0;
}
