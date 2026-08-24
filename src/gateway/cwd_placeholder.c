/*
 * cwd_placeholder.c — canonical C11 port of gateway/cwd_placeholder.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: gateway_cwd_placeholder_u_truthy_env @ gateway/cwd_placeholder.py:_truthy_env */
/* Python: (value or "").strip().lower() in {"true","1","yes"}. */
int gateway_cwd_placeholder_u_truthy_env(const char *arg) {
    if (!arg) return 0;
    /* strip + lowercase into a small stack buffer */
    char buf[64];
    char *q = buf;
    for (const char *p = arg; *p && q < buf + sizeof(buf) - 1; p++) {
        if (*p == ' ' || *p == '\\t' || *p == '\\n' || *p == '\\r') continue;
        *q++ = (char)tolower((unsigned char)*p);
    }
    *q = '\\0';
    return strcmp(buf, "true") == 0 || strcmp(buf, "1") == 0 || strcmp(buf, "yes") == 0;
}

/* PoP: gateway_cwd_placeholder_resolve_placeholder_terminal_cwd @ gateway/cwd_placeholder.py:resolve_placeholder_terminal_cwd */
int gateway_cwd_placeholder_resolve_placeholder_terminal_cwd(const char *arg) {
    /* Python: backend/mount resolution. Arg =
     * "backend\tplaceholder\tmount\tmessaging_cwd\thome\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    const char *t5 = t4 ? strchr(t4 + 1, '\t') : NULL;
    const char *backend = arg;
    int is_placeholder = t1 && t1[1] == '1';
    int mount = t2 && t2[1] == '1';
    const char *messaging = t3 ? t3 + 1 : "";
    const char *home = t4 ? t4 + 1 : "";
    if (!is_placeholder) { printf("%s\n", arg); return 0; }
    if (strcmp(backend, "local") == 0) { printf("%s\n", messaging[0] ? messaging : home); return 0; }
    if (strcmp(backend, "docker") == 0 && mount && messaging[0]) { printf("%s\n", messaging); return 0; }
    printf("\n");
    return 0;
}
