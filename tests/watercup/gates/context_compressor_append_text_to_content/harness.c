/* gate: context_compressor_append_text_to_content
 * Traced copy of src/agent/context_compressor_ports.c::
 * cc_append_text_to_content.
 * LIVE twin: agent/context_compressor.py::_append_text_to_content.
 *
 * Input argv token encoding (one per input): "<prepend>\t<content_json>"
 *   prepend: '1' to prepend, otherwise append; text is the fixed literal
 *            WCUP_NOTE on both sides
 *
 * Branch ladder (twin.py mirrors EXACTLY, same rung order):
 *   B1: no-content trigger (null / empty-string / empty-list encodings)
 *   B2: list-shaped content -> insert a text block
 *   B3: string-shaped content -> concatenate
 *   VSTR: result kind ("scalar" | "blocks" | "opaque")
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

static const char *TEXT = "WCUP_NOTE";

/* faithful traced copy of the port body; returns malloc'd JSON-ish result */
static char *append_port(const char *cj, const char *text, bool prepend) {
    if (!text) text = "";
    if (!cj || strcmp(cj, "[]") == 0 ||
        strcmp(cj, "\"\"") == 0 || strcmp(cj, "null") == 0) {
        char *out = NULL;
        asprintf(&out, "\"%s\"", text);
        return out;
    }
    if (cj[0] == '[') {
        size_t len = strlen(cj);
        char *out = malloc(len + strlen(text) + 64);
        if (!out) return strdup(cj);
        if (prepend)
            snprintf(out, len + strlen(text) + 64,
                     "[{\"type\":\"text\",\"text\":\"%s\"},%s", text, cj + 1);
        else
            snprintf(out, len + strlen(text) + 64,
                     "%.*s,{\"type\":\"text\",\"text\":\"%s\"}]",
                     (int)(len - 1), cj, text);
        return out;
    }
    if (cj[0] == '"') {
        size_t len = strlen(cj);
        char *out = malloc(len + strlen(text) + 8);
        if (!out) return strdup(cj);
        if (prepend)
            snprintf(out, len + strlen(text) + 8, "\"%s%.*s\"", text,
                     (int)(len - 2), cj + 1);
        else
            snprintf(out, len + strlen(text) + 8, "\"%.*s%s\"",
                     (int)(len - 2), cj + 1, text);
        return out;
    }
    return strdup(cj);
}

static void run_one(const char *arg) {
    WCUP_BEGIN("_append_text_to_content");

    const char *tab = strchr(arg, '\t');
    bool prepend = arg[0] == '1';
    const char *cj = tab ? tab + 1 : "";

    WCUP_BRANCH(!cj || strcmp(cj, "[]") == 0 ||
                strcmp(cj, "\"\"") == 0 || strcmp(cj, "null") == 0); /* B1 */
    WCUP_BRANCH(cj[0] == '[');                                      /* B2 */
    WCUP_BRANCH(cj[0] == '"');                                      /* B3 */
    char *res = append_port(cj, TEXT, prepend);
    const char *kind =
        (cj[0] == '[') ? "blocks" :
        (cj[0] == '"' || !cj[0] || strcmp(cj, "[]") == 0 ||
         strcmp(cj, "\"\"") == 0 || strcmp(cj, "null") == 0) ? "scalar" : "opaque";
    WCUP_VALSTR(kind);
    free(res);

    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_append_text_to_content");
    }
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s cc_append_text_to_content <p\\tcj...>\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
