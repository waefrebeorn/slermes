/* gate: delegate_looks_like_error_output — traced copy of
 * src/tools/delegate.c::delegate_looks_like_error_output
 * (LIVE twin: tools/delegate_tool.py::_looks_like_error_output).
 * Input is the tool-result preview STRING (stringify is identity here).
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1: empty content -> false
 *   B2: head starts { or [  -> JSON probe
 *     B3: parsed top-level object
 *       B4: has truthy "error" key -> true
 *       B5: "status" in {error,failed,failure,timeout} -> true
 *   B6: first line starts classic error marker -> result
 */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "watercup.h"

/* naive top-level scan inside a {...} body for `"key":` followed by a
 * non-null JSON value; extracts a string value for "status". */
static bool json_probe(const char *body, const char *key,
                       char *val_out, size_t val_sz) {
    size_t klen = strlen(key);
    const char *p = body;
    while ((p = strchr(p, '"')) != NULL) {
        p++;
        if (strncmp(p, key, klen) == 0 && p[klen] == '"') {
            const char *q = p + klen + 1;
            while (*q == ' ' || *q == ':') q++;
            if (*q == 'n') return false;            /* null value */
            if (*q == '"') {
                q++;
                size_t i = 0;
                while (*q && *q != '"' && i + 1 < val_sz)
                    val_out[i++] = ((*q == '\\' && q[1]) ? *++q : *q), q++;
                val_out[i] = '\0';
                return true;
            }
            /* truthy non-string scalar (number/true/object/array) */
            val_out[0] = '\0';
            return true;
        }
        while (*p && *p != '"') p++;
    }
    val_out[0] = '\0';
    return false;
}

static void wc_looks_like_error(const char *content) {
    WCUP_BEGIN("looks_like_error_output");
    bool result = false;
    if (!content || !*content) {                 /* B1 */
        WCUP_BRANCH(1);
        goto emit;
    }
    WCUP_BRANCH(0);
    {
        const char *head = content;
        while (*head == ' ' || *head == '\t' || *head == '\n' || *head == '\r') head++;
        if (head[0] == '{' || head[0] == '[') {  /* B2 */
            WCUP_BRANCH(1);
            bool is_obj = (head[0] == '{');
            if (is_obj) {                        /* B3 */
                WCUP_BRANCH(1);
                char val[256];
                if (json_probe(head + 1, "error", val, sizeof val)) {
                    WCUP_BRANCH(1);              /* B4 -> true */
                    result = true;
                    goto emit;
                }
                WCUP_BRANCH(0);
                if (json_probe(head + 1, "status", val, sizeof val)) {
                    bool bad = (strcmp(val, "error") == 0 || strcmp(val, "failed") == 0 ||
                                strcmp(val, "failure") == 0 || strcmp(val, "timeout") == 0);
                    if (bad) {
                        WCUP_BRANCH(1);          /* B5 -> true */
                        result = true;
                        goto emit;
                    }
                    WCUP_BRANCH(0);
                } else {
                    WCUP_BRANCH(0);
                }
            } else {
                WCUP_BRANCH(0);
            }
        } else {
            WCUP_BRANCH(0);
        }
    }
    {
        /* first non-empty-line prefix check on the raw first line */
        char line[512];
        const char *nl = strchr(content, '\n');
        size_t len = nl ? (size_t)(nl - content) : strlen(content);
        if (len >= sizeof line) len = sizeof line - 1;
        memcpy(line, content, len);
        line[len] = '\0';
        char *l = line;
        while (*l == ' ' || *l == '\t') l++;
        bool marker = (strncmp(l, "error:", 6) == 0 ||
                       strncmp(l, "failed:", 7) == 0 ||
                       strncmp(l, "traceback", 8) == 0 ||
                       strncmp(l, "exception:", 10) == 0);
        result = marker;                         /* B6 */
        WCUP_BRANCH(result);
    }
emit:
    WCUP_VALSTR(result ? "True" : "False");
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("looks_like_error_output");
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s looks_like_error_output <content>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) wc_looks_like_error(argv[i]);
    return 0;
}
