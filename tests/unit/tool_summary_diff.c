/* Harness: print compress_summarize_tool's output for one (tool, args, content) triple.
 *
 * Reads the triple from stdin as three NUL-free fields separated by TABs, one per line, and prints
 * "IN <name>" / "OUT <summary>" pairs. Linking only libjson keeps the harness honest: it exercises
 * exactly the function the product calls, not a copy.
 *
 * usage: tool_summary_diff < cases.tsv
 */
#include "context_compressor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Field 3 arrives escaped so a payload with newlines survives the line protocol. */
static void unescape_in_place(char *s) {
    char *w = s;
    for (char *r = s; *r; r++) {
        if (*r == '\\' && (r[1] == 'n' || r[1] == 't' || r[1] == '\\')) {
            r++;
            *w++ = (*r == 'n') ? '\n' : (*r == 't' ? '\t' : '\\');
        } else {
            *w++ = *r;
        }
    }
    *w = '\0';
}

int main(void) {
    char *line = NULL;
    size_t cap = 0;
    ssize_t n = 0;
    static char out[8192];

    while ((n = getline(&line, &cap, stdin)) >= 0) {
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
        if (n == 0) continue;

        char *name = line;
        char *args = strchr(line, '\t');
        if (!args) continue;
        *args++ = '\0';
        char *content = strchr(args, '\t');
        if (content) *content++ = '\0';
        else content = "";
        if (*content) unescape_in_place(content);

        out[0] = '\0';
        compress_summarize_tool(name, args, content, out, sizeof(out));
        printf("OUT %s\n", out);
    }
    free(line);
    return 0;
}
