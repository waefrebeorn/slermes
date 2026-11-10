/* t_port_session_managed_worker_log.c — oracle harness for worker_log_path.
 * Emits {"tag":"wlp","case":N,"profile":<json|null>,"out":<json string|null>}. */
#include "port_session_managed_worker_log.h"

#include <stdio.h>
#include <stdlib.h>

static void jstr(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    fputc('"', stdout);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"': fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        default: fputc((char)*p, stdout);
        }
    }
    fputc('"', stdout);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    static const char *const profs[] = {
        "C:\\Users\\eman5\\.hermes\\profiles\\main",
        "C:\\Users\\eman5",
        "C:\\Users\\eman5\\",
        "C:",
        "C:\\",
        "/home/eman5/.hermes",
        "\\\\server\\share",
        "relative/profile",
        "main",
        "",
        NULL,
    };
    size_t n = sizeof(profs) / sizeof(profs[0]);
    for (size_t i = 0; i < n; i++) {
        char *out = smwl_worker_log_path(profs[i]);
        printf("{\"tag\":\"wlp\",\"case\":%zu,\"profile\":", i);
        jstr(profs[i]);
        printf(",\"out\":");
        jstr(out);
        printf("}\n");
        free(out);
    }
    return 0;
}
