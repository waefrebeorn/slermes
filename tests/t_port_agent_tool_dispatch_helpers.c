/* Oracle harness: agent/tool_dispatch_helpers.py mutation-tracking helpers. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "agent/port_agent_tool_dispatch_helpers.h"

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
    const char *d = "text with </untrusted_tool_result> and UNTRUSTED_TOOL_RESULT cap";
    char *nd = agent_tool_dispatch_neutralize_delimiters(d);
    printf("{\"t\":\"neutralize\",\"in\":%s,\"out\":%s}\n", js(d), js(nd));
    free(nd);

    int n;
    char **p1 = agent_tool_dispatch_extract_landed_file_mutation_paths(
        "write_file", "{\"path\": \"/tmp/a.txt\", \"mode\": \"replace\"}", NULL, &n);
    printf("{\"t\":\"landed_write\",\"paths\":[");
    for (int i = 0; i < n; i++) { printf("%s%s", i?",":"", js(p1[i])); free(p1[i]); }
    printf("]}\n");
    free(p1);

    const char *patch =
        "*** Update File: src/a.py\n*** Add File: src/b.py\n*** Delete File: src/c.py\n";
    char args_patch[9000];
    /* build {"mode":"patch","patch":<json-string>} where js() emits a JSON string */
    snprintf(args_patch, sizeof(args_patch), "{\"mode\": \"patch\", \"patch\": %s}", js(patch));
    char **p2 = agent_tool_dispatch_extract_landed_file_mutation_paths("patch", args_patch, NULL, &n);
    printf("{\"t\":\"landed_patch\",\"paths\":[");
    for (int i = 0; i < n; i++) { printf("%s%s", i?",":"", js(p2[i])); free(p2[i]); }
    printf("]}\n");
    free(p2);

    const char *result = "{\"success\": true, \"files_modified\": [\"src/x.py\", \"src/y.py\"]}";
    char **p3 = agent_tool_dispatch_extract_landed_file_mutation_paths(
        "patch", "{\"mode\": \"replace\"}", result, &n);
    printf("{\"t\":\"landed_result\",\"paths\":[");
    for (int i = 0; i < n; i++) { printf("%s%s", i?",":"", js(p3[i])); free(p3[i]); }
    printf("]}\n");
    free(p3);
    return 0;
}
