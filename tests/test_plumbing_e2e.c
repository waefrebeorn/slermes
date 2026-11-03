/* End-to-end plumbing proof: pull the real registry the binary uses and
 * dispatch actual tools through it. Proves registration + dispatch + handler
 * invocation are wired, not just compiled. */
#include "registry.h"
#include "hermes_agent.h"
#include <stdio.h>
#include <string.h>

static int pass, fail;
static void ck(const char *n, int c) {
    printf("  %s: %s\n", c ? "PASS" : "FAIL", n);
    c ? pass++ : fail++;
}

int main(void)
{
    printf("=== registry plumbing (end to end) ===\n");

    /* The binary calls this before anything touches the registry; without it
     * the registry holds only its initial entry. */
    extern void tools_init_all(void);
    tools_init_all();

    tool_registry_t *reg = get_registry();
    ck("registry non-NULL", reg != NULL);
    if (!reg) { printf("\nResults: 0 passed, 1 failed\n"); return 1; }

    ck("registry has tools", reg->count > 50);
    printf("  (registry count = %d)\n", reg->count);

    /* discover a few by name */
    const char *want[] = {"read_file", "write_file", "terminal", "search_files"};
    for (int i = 0; i < 4; i++) {
        char label[64];
        snprintf(label, sizeof label, "registered: %s", want[i]);
        ck(label, registry_find(want[i]) != NULL);
    }

    /* real dispatch: read_file on a file we know exists */
    char *out = registry_dispatch("read_file",
        "{\"path\":\"C:/Users/eman5/slermes/README.md\",\"limit\":1}", "plumb");
    ck("read_file returns", out != NULL);
    if (out) {
        ck("read_file content is not an error",
           strstr(out, "\"error\"") == NULL);
        ck("read_file returned real text",
           strstr(out, "slermes") != NULL || strstr(out, "Slermes") != NULL);
        printf("  (first 120 chars) %.120s\n", out);
        free(out);
    }

    /* unknown tool must fail cleanly, not crash */
    char *bad = registry_dispatch("definitely_not_a_tool", "{}", "plumb");
    ck("unknown tool returns an error object",
       bad && strstr(bad, "error") != NULL);
    free(bad);

    /* write_file -> read_file round trip proves the fs path end to end */
    const char *tmp = "C:/Users/eman5/AppData/Local/hermes/cache/scratch/plumb_probe.txt";
    char wargs[512];
    snprintf(wargs, sizeof wargs,
             "{\"path\":\"%s\",\"content\":\"plumbing-ok-123\"}", tmp);
    char *w = registry_dispatch("write_file", wargs, "plumb");
    ck("write_file succeeds", w && strstr(w, "error") == NULL);
    if (w) printf("  (write_file said) %.160s\n", w);
    free(w);

    char rargs[512];
    snprintf(rargs, sizeof rargs, "{\"path\":\"%s\"}", tmp);
    char *r = registry_dispatch("read_file", rargs, "plumb");
    ck("round trip returns the written content",
       r && strstr(r, "plumbing-ok-123") != NULL);
    free(r);

    /* terminal must actually execute and return stdout */
    char *t = registry_dispatch("terminal",
        "{\"command\":\"echo plumb-terminal-ok\"}", "plumb");
    ck("terminal returns", t != NULL);
    ck("terminal executed the command",
       t && strstr(t, "plumb-terminal-ok") != NULL);
    free(t);

    printf("\nResults: %d passed, %d failed\n", pass, fail);
    return fail ? 1 : 0;
}
