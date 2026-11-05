/* t_port_delegation_registry.c — oracle harness for the pure core of
 * agent/delegation_registry.py: _bundled_disabled and backend_status reason. */
#include "port_delegation_registry.h"

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

    /* _bundled_disabled: name vs a disabled-list (space/comma separated). */
    static const char *const names[] = { "default", "subagents", "codex", "x" };
    static const char *const lists[] = {
        NULL, "", "default", "delegation/default", "codex,deepseek",
        " subagents delegation/subagents ", "delegation/default default",
    };
    for (size_t n = 0; n < sizeof(names) / sizeof(names[0]); n++) {
        for (size_t i = 0; i < sizeof(lists) / sizeof(lists[0]); i++) {
            printf("{\"family\":\"bd\",\"case\":\"bd_%zu_%zu\",\"name\":", n, i);
            jstr(names[n]);
            printf(",\"list\":");
            jstr(lists[i]);
            printf(",\"out\":%d}\n",
                   dr_bundled_disabled(names[n], lists[i]) ? 1 : 0);
        }
    }

    /* backend_status reason: (name, available, registered) -> message. */
    static const char *const bname[] = { "default", "codex", "subagents" };
    static const int flags[][2] = {
        {0, 0}, {0, 1}, {1, 0}, {1, 1},
    };
    for (size_t n = 0; n < sizeof(bname) / sizeof(bname[0]); n++) {
        for (size_t f = 0; f < sizeof(flags) / sizeof(flags[0]); f++) {
            char *out = dr_backend_status_reason(bname[n], flags[f][0],
                                                 flags[f][1]);
            printf("{\"family\":\"bs\",\"case\":\"bs_%zu_%zu\",\"name\":", n, f);
            jstr(bname[n]);
            printf(",\"available\":%d,\"registered\":%d,\"out\":",
                   flags[f][0], flags[f][1]);
            jstr(out);
            printf("}\n");
            free(out);
        }
    }
    return 0;
}
