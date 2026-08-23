/* gate: context_compressor_append_text_to_content — traced copy of
 * src/agent/context_compressor_ports.c::cc_append_text_to_content.
 * LIVE twin: agent/context_compressor.py::_append_text_to_content.
 *
 * Input encoding (single argv token per case): "<content_json>\t<text>\t<prepend 0|1>"
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1: content null/empty-sentinel -> text as bare string
 *   B2: content is a list
 *     B3: prepend vs append inside the list
 *   B4: content is a string
 *     B5: prepend vs append on the string
 *   else: passthrough unchanged
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

static char *wc_append_text_to_content(const char *content_json, const char *text,
                                       bool prepend) {
    WCUP_BEGIN("append_text_to_content");
    char *out = NULL;
    if (!text) text = "";
    if (!content_json || strcmp(content_json, "[]") == 0 ||
        strcmp(content_json, "\"\"") == 0 || strcmp(content_json, "null") == 0) {
        WCUP_BRANCH(1);                              /* B1 */
        asprintf(&out, "\"%s\"", text);
        goto done;
    }
    WCUP_BRANCH(0);
    if (content_json[0] == '[') {                    /* B2 */
        WCUP_BRANCH(1);
        size_t len = strlen(content_json);
        out = malloc(len + strlen(text) + 64);
        if (!out) { out = strdup(content_json); goto done; }
        if (prepend) {                               /* B3 */
            WCUP_BRANCH(1);
            snprintf(out, len + strlen(text) + 64,
                     "[{\"type\":\"text\",\"text\":\"%s\"},%s", text, content_json + 1);
        } else {
            WCUP_BRANCH(0);
            snprintf(out, len + strlen(text) + 64,
                     "%.*s,{\"type\":\"text\",\"text\":\"%s\"}]",
                     (int)(len - 1), content_json, text);
        }
        goto done;
    }
    WCUP_BRANCH(0);
    if (content_json[0] == '"') {                    /* B4 */
        WCUP_BRANCH(1);
        size_t len = strlen(content_json);
        out = malloc(len + strlen(text) + 8);
        if (!out) { out = strdup(content_json); goto done; }
        if (prepend) {                               /* B5 */
            WCUP_BRANCH(1);
            snprintf(out, len + strlen(text) + 8, "\"%s%.*s\"",
                     text, (int)(len - 2), content_json + 1);
        } else {
            WCUP_BRANCH(0);
            snprintf(out, len + strlen(text) + 8, "\"%.*s%s\"",
                     (int)(len - 2), content_json + 1, text);
        }
        goto done;
    }
    WCUP_BRANCH(0);                                  /* passthrough */
    out = strdup(content_json);
done: {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("append_text_to_content");
        return out;
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s append_text_to_content <content>\\t<text>\\t<prep>...\n", argv[0]);
        return 2;
    }
    for (int i = 2; i < argc; i++) {
        char *arg = strdup(argv[i]);
        char *t1 = strchr(arg, '\t');
        char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
        if (!t1 || !t2) { fprintf(stderr, "bad input: %s\n", argv[i]); free(arg); continue; }
        *t1 = '\0'; *t2 = '\0';
        bool prepend = t2[1] == '1';
        wc_append_text_to_content(strcmp(arg, "NULL") == 0 ? NULL : arg,
                                  t1 + 1, prepend);
        free(arg);
    }
    return 0;
}
