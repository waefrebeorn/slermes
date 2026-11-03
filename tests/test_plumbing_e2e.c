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

    /* Real dispatch: read_file on README.md in the current working directory.
     * Derived from getcwd() rather than USERPROFILE because the suite runs the
     * binary under MSYS, where USERPROFILE points at /c/msys64/home/... and the
     * repo lives elsewhere. This still exercises the Windows-vs-POSIX
     * canonicalisation in is_safe_path, since getcwd returns a native path. */
    char cwd[512];
    if (!getcwd(cwd, sizeof cwd)) snprintf(cwd, sizeof cwd, ".");
    /* Normalise immediately after getcwd: these paths are embedded in JSON
     * argument strings, where a raw backslash is an escape character and
     * silently eats the separator ("C:Userseman5..."). */
    for (char *q = cwd; *q; q++) if (*q == '\\') *q = '/';
    char readme[640];
    snprintf(readme, sizeof readme, "%s/README.md", cwd);
    printf("  (reading %s)\n", readme);

    char rargs_q[700];
    snprintf(rargs_q, sizeof rargs_q, "{\"path\":\"%s\",\"limit\":1}", readme);
    char *out = registry_dispatch("read_file", rargs_q, "plumb");
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
    /* Probe next to cwd: the sandbox only permits HOME/USERPROFILE/TEMP/TMP and
     * the working directory. The MSYS suite has USERPROFILE=/c/msys64/home/...
     * while TEMP=/tmp, so an AppData path is correctly refused there. */
    char tmp[640];
    snprintf(tmp, sizeof tmp, "%s/plumb_probe.txt", cwd);
    char wargs[800];
    snprintf(wargs, sizeof wargs,
             "{\"path\":\"%s\",\"content\":\"plumbing-ok-123\"}", tmp);
    char *w = registry_dispatch("write_file", wargs, "plumb");
    ck("write_file succeeds", w && strstr(w, "error") == NULL);
    if (w) printf("  (write_file said) %.160s\n", w);
    free(w);

    char rargs[800];
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

    /* search_files — core navigation tool, same POSIX-path risk as read_file */
    char sargs[900];
    snprintf(sargs, sizeof sargs,
             "{\"pattern\":\"SLERMEd\",\"path\":\"%s/README.md\"}", cwd);
    char *s = registry_dispatch("search_files", sargs, "plumb");
    ck("search_files returns", s != NULL);
    ck("search_files found the text",
       s && strstr(s, "SLERMEd") != NULL);
    if (s && strstr(s, "error")) printf("  (search said) %.160s\n", s);
    free(s);

    /* patch — replaces text in place; uses its own atomic write path */
    char pargs[900];
    snprintf(pargs, sizeof pargs,
             "{\"path\":\"%s\",\"old_string\":\"plumbing-ok-123\","
             "\"new_string\":\"plumbing-ok-456\"}", tmp);
    char *p = registry_dispatch("patch", pargs, "plumb");
    ck("patch returns", p != NULL);
    ck("patch did not error", p == NULL || strstr(p, "error") == NULL);
    if (p && strstr(p, "error")) printf("  (patch said) %.160s\n", p);
    free(p);

    char *r2 = registry_dispatch("read_file", rargs, "plumb");
    ck("patch actually changed the file",
       r2 && strstr(r2, "plumbing-ok-456") != NULL);
    free(r2);

    printf("\nResults: %d passed, %d failed\n", pass, fail);
    return fail ? 1 : 0;
}
