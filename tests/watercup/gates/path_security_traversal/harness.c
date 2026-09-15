/* gate: path_security_traversal — traced copy of
 * src/tools/path_security.c::cli_tools_path_security_has_traversal_component
 * (LIVE twin: tools/path_security.py::has_traversal_component).
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   per path component (in order): B(component == "..")
 *   VSTR(final result "1"/"0")
 *
 * The C port scans chars; the LIVE twin walks Path.parts. The rung
 * contract is one B event per component, compared for equality with "..".
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"

static int wc_has_traversal(const char *path_str)
{
    WCUP_BEGIN("path_security_traversal");
    int found = 0;
    const char *p = path_str ? path_str : "";

    while (*p) {
        while (*p == '/') p++;          /* skip separators (empty parts) */
        if (!*p) break;
        const char *start = p;
        while (*p && *p != '/') p++;
        size_t len = (size_t)(p - start);
        /* pathlib drops "." components during normalization — so does the
         * LIVE Path.parts walk the twin mirrors. */
        if (len == 1 && start[0] == '.') continue;
        int is_dotdot = (len == 2 && start[0] == '.' && start[1] == '.');
        WCUP_BRANCH(is_dotdot);
        if (is_dotdot) { found = 1; break; }
    }

    char res[8];
    snprintf(res, sizeof res, "%d", found);
    WCUP_VALSTR(res);

    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("path_security_traversal");
    return found;
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s fn <path>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++)
        wc_has_traversal(argv[i]);
    return 0;
}
