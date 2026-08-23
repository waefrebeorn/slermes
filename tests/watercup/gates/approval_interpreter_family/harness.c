/* gate: approval_interpreter_family — traced C copy of
 * src/tools/approval_wrappers.c::appr_u_interpreter_family.
 * PoP of LIVE tools/approval.py::_interpreter_family (regex ladder).
 *
 * Known documented divergences (inputs below avoid them):
 *  - C matches by prefix/strcmp (case-sensitive), LIVE by fullmatch regex on
 *    basename.lower(): uppercase basenames ("Python3") and over-broad prefixes
 *    ("py2", "python4", "pythonx") diverge.
 */
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "watercup.h"

static void interpreter_family(const char *arg) {
    WCUP_BEGIN("_interpreter_family");
    const char *res = NULL;
    char base[128];
    base[0] = '\0';
    if (!arg || !*arg) {
        WCUP_BRANCH(1);
    } else {
        WCUP_BRANCH(0);
        const char *name = arg;
        const char *slash = strrchr(name, '/');
        if (slash) name = slash + 1;
        snprintf(base, sizeof(base), "%s", name);
        size_t blen = strlen(base);
        if (blen >= 4 && strcasecmp(base + blen - 4, ".exe") == 0) base[blen - 4] = '\0';
        WCUP_VALSTR(base);
        /* python rung */
        int is_py = strcmp(base, "py") == 0 || strcmp(base, "python") == 0 ||
                    strcmp(base, "python2") == 0 || strcmp(base, "python3") == 0 ||
                    strncmp(base, "python", 6) == 0 || strncmp(base, "py", 2) == 0;
        WCUP_BRANCH(is_py);
        if (is_py) res = "python";
        if (!res) {
            int is_node = strcmp(base, "node") == 0 || strcmp(base, "nodejs") == 0;
            WCUP_BRANCH(is_node);
            if (is_node) res = "node";
        }
        if (!res) {
            int is_perl = strcmp(base, "perl") == 0 || strncmp(base, "perl", 4) == 0;
            WCUP_BRANCH(is_perl);
            if (is_perl) res = "perl";
        }
        if (!res) {
            int is_ruby = strcmp(base, "ruby") == 0 || strncmp(base, "ruby", 4) == 0;
            WCUP_BRANCH(is_ruby);
            if (is_ruby) res = "ruby";
        }
        if (!res) {
            int is_php = strcmp(base, "php") == 0;
            WCUP_BRANCH(is_php);
            if (is_php) res = "php";
        }
        if (!res) {
            int is_ps = strcmp(base, "powershell") == 0 || strcmp(base, "pwsh") == 0;
            WCUP_BRANCH(is_ps);
            if (is_ps) res = "powershell";
        }
    }
    WCUP_VALSTR(res ? res : "");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_interpreter_family");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _interpreter_family <exe>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) interpreter_family(argv[i]);
    return 0;
}
