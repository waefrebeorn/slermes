/* gate: code_execution_tool_sandbox_failure_hint — traced copy of
 * src/tools/code_execution_tool.c::cet_sandbox_failure_hint
 * (LIVE twin: tools/code_execution_tool.py::_sandbox_failure_hint).
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B0: empty stderr                  -> None
 *   V : window (stderr[:4000])
 *   B1: 'cannot import name .. from hermes_tools'
 *     V : missing name
 *     B2: missing in BUILTIN_HELPERS  -> built-in hint
 *         else                        -> not-available hint (avail list NOT
 *         hashed: C emits declaration order, LIVE emits sorted() — known,
 *         documented text-order divergence; rungs are identical)
 *   B3: NameError json_parse|shell_quote|retry -> built-in hint
 *   B4: ModuleNotFoundError No module named X   -> not-installed hint
 *   B5: TypeError-str-indices / AttributeError-str-get -> dict hint
 *   else -> None
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <stdbool.h>
#include "watercup.h"

static const char *BUILTIN_HELPERS[] = {"json_parse", "shell_quote", "retry", NULL};

/* faithful traced copy of cet_sandbox_failure_hint's control structure */
static char *wc_hint(const char *stderr_text, const char *enabled_tools_csv) {
    char *hint = NULL;
    const char *missing = NULL, *name1 = NULL;
    int b2 = 0;
    char window[4001];
    regex_t re;
    regmatch_t pm[2];
    WCUP_BEGIN("cet_sandbox_failure_hint");

    if (!stderr_text || !*stderr_text) { WCUP_BRANCH(1); goto out; }  /* B0 */
    WCUP_BRANCH(0);
    strncpy(window, stderr_text, 4000);
    window[4000] = '\0';
    WCUP_VALSTR(window);

    /* pattern 1 */
    {
        bool m1 = false;
        if (regcomp(&re, "cannot import name '([A-Za-z0-9_]+)' from 'hermes_tools'", REG_EXTENDED) == 0) {
            if (regexec(&re, window, 2, pm, 0) == 0 && pm[1].rm_so != -1) {
                static char miss[128];
                int n = pm[1].rm_eo - pm[1].rm_so;
                if (n > 127) n = 127;
                memcpy(miss, window + pm[1].rm_so, n); miss[n] = '\0';
                missing = miss;
                m1 = true;
            }
            regfree(&re);
        }
        if (!m1) { WCUP_BRANCH(0); goto p2; }                             /* B1 */
        WCUP_BRANCH(1);
        WCUP_VALSTR(missing);
        {
            bool is_builtin = false;
            int i;
            for (i = 0; BUILTIN_HELPERS[i]; i++)
                if (strcmp(missing, BUILTIN_HELPERS[i]) == 0) { is_builtin = true; break; }
            b2 = is_builtin ? 1 : 0;
            WCUP_BRANCH(b2);                                              /* B2 */
        }
        if (b2) goto out;   /* built-in hint path */
        goto out;           /* not-available hint path */

p2:
        /* pattern 2 */
        {
            bool m2 = false;
            if (regcomp(&re, "NameError: name '(json_parse|shell_quote|retry)' is not defined", REG_EXTENDED) == 0) {
                if (regexec(&re, window, 2, pm, 0) == 0 && pm[1].rm_so != -1) {
                    static char nm[64];
                    int n = pm[1].rm_eo - pm[1].rm_so;
                    if (n > 63) n = 63;
                    memcpy(nm, window + pm[1].rm_so, n); nm[n] = '\0';
                    name1 = nm;
                    m2 = true;
                }
                regfree(&re);
            }
            if (!m2) { WCUP_BRANCH(0); goto p3; }                         /* B3 */
            WCUP_BRANCH(1);
            WCUP_VALSTR(name1);
            goto out;
p3:
            /* pattern 3 */
            {
                bool m3 = false;
                static char modname[256];
                if (regcomp(&re, "ModuleNotFoundError: No module named '([A-Za-z0-9_.]+)'", REG_EXTENDED) == 0) {
                    if (regexec(&re, window, 2, pm, 0) == 0 && pm[1].rm_so != -1) {
                        int n = pm[1].rm_eo - pm[1].rm_so;
                        if (n > 255) n = 255;
                        memcpy(modname, window + pm[1].rm_so, n); modname[n] = '\0';
                        m3 = true;
                    }
                    regfree(&re);
                }
                if (!m3) { WCUP_BRANCH(0); goto p4; }                     /* B4 */
                WCUP_BRANCH(1);
                WCUP_VALSTR(modname);
                goto out;
p4:
                /* pattern 4: str-index / attr-get misuse */
                {
                    bool m4 = false;
                    if (regcomp(&re, "TypeError: string indices must be integers|AttributeError: 'str' object has no attribute 'get'", REG_EXTENDED) == 0) {
                        m4 = regexec(&re, window, 0, NULL, 0) == 0;
                        regfree(&re);
                    }
                    WCUP_BRANCH(m4 ? 1 : 0);                              /* B5 */
                }
            }
        }
    }
out:
    WCUP_VALSTR(hint ? hint : "<none>");
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cet_sandbox_failure_hint");
        free(hint);
        return NULL;
    }
}

int main(int argc, char **argv) {
    int i;
    if (argc < 3) {
        fprintf(stderr, "usage: %s cet_sandbox_failure_hint <stderr> [enabled_tools_csv]\n", argv[0]);
        return 2;
    }
    for (i = 2; i < argc; i++)
        wc_hint(argv[i], "");
    return 0;
}
