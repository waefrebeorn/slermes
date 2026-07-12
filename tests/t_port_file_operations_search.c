/* Oracle harness: tools/file_operations.py search-diagnostics helpers. One JSON per line. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tools/port_file_operations_search.h"

static const char *js(const char *s) {
    static char bufs[8][8192];
    static int bi = 0;
    char *b = bufs[bi]; bi = (bi + 1) % 8;
    char *q = b; *q++ = '"';
    for (const char *p = s; p && *p && q - b < 8000; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') { *q++ = '\\'; *q++ = c; }
        else if (c == '\n') { *q++ = '\\'; *q++ = 'n'; }
        else *q++ = c;
    }
    *q++ = '"'; *q = '\0';
    return b;
}

int main(void) {
    file_ops_execute_result_t r1 = {124, "line1\n[Command timed out after 30s]\nline2", NULL};
    char *reason = NULL;
    char *s1 = file_ops_search_search_stdout_and_limit(&r1, &reason);
    printf("{\"t\":\"search_limit\",\"stdout\":%s,\"reason\":%s}\n", js(s1), js(reason?reason:""));
    free(s1); free(reason);

    file_ops_execute_result_t r2 = {0, "normal output", NULL};
    char *reason2 = NULL;
    char *s2 = file_ops_search_search_stdout_and_limit(&r2, &reason2);
    printf("{\"t\":\"search_limit2\",\"stdout\":%s,\"reason\":%s}\n", js(s2), reason2 ? js(reason2) : "null");
    free(s2); free(reason2);

    const char *out =
        "rg: /foo: Permission denied\n"
        "src/main.c:10:int main()\n"
        "grep: Invalid regular expression\n"
        "src/util.c:5:void x()\n"
        "--\n"
        "somepath/file-12-name.py-8-context";
    char *diag = NULL, *pay = NULL;
    file_ops_search_split_tool_diagnostics(out, &diag, &pay);
    printf("{\"t\":\"split_diag\",\"diag\":%s,\"pay\":%s}\n", js(diag), js(pay));
    free(diag); free(pay);
    return 0;
}
