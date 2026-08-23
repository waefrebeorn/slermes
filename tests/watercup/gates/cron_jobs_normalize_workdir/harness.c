/* gate: cron_jobs_normalize_workdir — traced copy of
 * src/cron/jobs.c::cronjobs_normalize_workdir
 * (LIVE twin: cron/jobs.py::_normalize_workdir).
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1: null / empty-after-strip -> None
 *   B2: not absolute after ~ expansion -> error
 *   B3: resolve() fails (missing path) -> error
 *   B4: not a directory -> error
 *   V1: resolved path | "ERR" | "None"
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/stat.h>
#include <stdbool.h>
#include "watercup.h"

static char *str_strip_dup(const char *s) {
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' ||
                       s[len-1] == '\n' || s[len-1] == '\r')) len--;
    char *out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}

static void wc_normalize_workdir(const char *workdir) {
    WCUP_BEGIN("normalize_workdir");
    const char *result = "None";
    if (!workdir) {                              /* B1 */
        WCUP_BRANCH(1);
        goto emit;
    }
    WCUP_BRANCH(0);
    {
        char *raw = str_strip_dup(workdir);
        if (!raw || !*raw) {                     /* B1b empty */
            free(raw);
            WCUP_BRANCH(1);
            goto emit;
        }
        WCUP_BRANCH(0);
        char expanded[PATH_MAX];
        if (raw[0] == '~') {
            const char *hdir = getenv("HOME");
            if (!hdir) hdir = "";
            snprintf(expanded, sizeof expanded, "%s%s", hdir, raw + 1);
        } else {
            snprintf(expanded, sizeof expanded, "%s", raw);
        }
        free(raw);
        if (expanded[0] != '/') {                /* B2 */
            WCUP_BRANCH(1);
            result = "ERR";
            goto emit;
        }
        WCUP_BRANCH(0);
        char resolved[PATH_MAX];
        if (!realpath(expanded, resolved)) {     /* B3 */
            WCUP_BRANCH(1);
            result = "ERR";
            goto emit;
        }
        WCUP_BRANCH(0);
        struct stat st;
        if (stat(resolved, &st) != 0 || !S_ISDIR(st.st_mode)) {  /* B4 */
            WCUP_BRANCH(1);
            result = "ERR";
            goto emit;
        }
        WCUP_BRANCH(0);
        result = resolved;
    }
emit:
    WCUP_VALSTR(result);
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("normalize_workdir");
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s normalize_workdir <workdir>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) wc_normalize_workdir(argv[i]);
    return 0;
}
