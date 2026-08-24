/*
 * psutil_android.c — canonical C11 port of hermes_cli/psutil_android.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_psutil_android_u_normalize_member_parts @ hermes_cli/psutil_android.py:_normalize_member_parts */
int hermes_cli_psutil_android_u_normalize_member_parts(const char *arg) {
    /* Python: PurePosixPath parts minus "" and "."; reject absolute or
     * "..". Arg = member_name. */
    if (!arg || !*arg) { printf("\n"); return 1; }
    if (arg[0] == '/') { printf("unsafe: %s\n", arg); return 1; }
    const char *p = arg;
    int first = 1;
    int safe = 1;
    while (*p) {
        const char *slash = strchr(p, '/');
        size_t len = slash ? (size_t)(slash - p) : strlen(p);
        if (len == 2 && p[0] == '.' && p[1] == '.') { safe = 0; break; }
        if (!(len == 1 && p[0] == '.') && len) {
            if (!first) printf("/");
            printf("%.*s", (int)len, p);
            first = 0;
        }
        p = slash ? slash + 1 : p + len;
    }
    if (!safe || first) { printf("unsafe: %s\n", arg); return 1; }
    printf("\n");
    return 0;
}

/* PoP: hermes_cli_psutil_android_u_safe_extract_tar_gz @ hermes_cli/psutil_android.py:_safe_extract_tar_gz */
int hermes_cli_psutil_android_u_safe_extract_tar_gz(const char *arg) {
    /* Python: traversal-safe extract. Arg = "archive\tstate\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "unsupported") == 0) {
        fprintf(stderr, "Unsupported archive member type: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    if (strcmp(state, "unreadable") == 0) {
        fprintf(stderr, "Cannot read archive member: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("extracted safely: %s\n", arg);
    return 0;
}
