/* t_port_compression_facade.c — oracle harness for the pure warning messages.
 * Emits {"tag","waited","ceiling"/"progress"/"idle","out"}. Emitted numbers are
 * %.17g so the oracle feeds the same value to Python's f-string. */
#include "port_compression_facade.h"

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

    /* _warn_commit_overrun */
    {
        static const double waits[] = { 3.4, 7.5, 50.2 };
        static const double ceilings[] = { 15.0, 20.0, 60.0 };
        for (size_t i = 0; i < sizeof(waits)/sizeof(waits[0]); i++) {
            char *o = cf_warn_commit_overrun(waits[i], ceilings[i]);
            printf("{\"tag\":\"warn\",\"waited\":%.17g,\"ceiling\":%.17g,\"out\":",
                   waits[i], ceilings[i]);
            jstr(o); printf("}\n");
            free(o);
        }
    }
    /* _report_compression_timeout total */
    {
        struct { double w; int p; } cases[] = { {10.0,1},{10.0,0},{2.34,1} };
        for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
            char *o = cf_commit_timeout_total(cases[i].w, cases[i].p);
            printf("{\"tag\":\"total\",\"waited\":%.17g,\"progress\":%s,\"out\":",
                   cases[i].w, cases[i].p ? "true" : "false");
            jstr(o); printf("}\n");
            free(o);
        }
    }
    /* _report_compression_timeout stalled */
    {
        static const double idles[] = { 3.4, 120.0 };
        for (size_t i = 0; i < sizeof(idles)/sizeof(idles[0]); i++) {
            char *o = cf_commit_timeout_stalled(idles[i]);
            printf("{\"tag\":\"stall\",\"idle\":%.17g,\"out\":", idles[i]);
            jstr(o); printf("}\n");
            free(o);
        }
    }
    return 0;
}
