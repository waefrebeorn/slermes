/* t_port_auth_commands.c — oracle harness for _display_source +
 * _classify_exhausted_status. Emits {"tag","source"/"code","reason","message","out"} */
#include "port_auth_commands.h"

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

    /* _display_source */
    {
        static const char *const srcs[] = {
            "manual:keyboard", "manual:api", "openai", "manual:", "manual:cli",
            "manual:AB C", "manual", "", NULL,
        };
        for (size_t i = 0; i < sizeof(srcs)/sizeof(srcs[0]); i++) {
            char *o = acm_display_source(srcs[i]);
            printf("{\"tag\":\"ds\",\"source\":");
            jstr(srcs[i]); printf(",\"out\":"); jstr(o); printf("}\n");
            free(o);
        }
    }
    /* _classify_exhausted_status: code, reason, message */
    {
        struct { const char *c; const char *r; const char *m; } cases[] = {
            {"429", "", ""}, {"401", "", ""}, {"200", "rate_limit", ""},
            {"200", "", "quota exceeded"}, {"200", "", ""}, {"503", "unauthorized_request", ""},
            {"403", "", "expired token"}, {"400", "AUTH FAILED", ""}, {"401", "", "UNAUTHORIZED"},
            {NULL, "invalid_token", ""}, {"200", "", "too many requests"},
            {NULL, "", ""}, {"429", "auth", ""}, {"307", "forbidden", "nope"},
        };
        for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
            char *o = acm_classify_exhausted(cases[i].c, cases[i].r, cases[i].m);
            printf("{\"tag\":\"classify\",\"code\":");
            jstr(cases[i].c);
            printf(",\"reason\":"); jstr(cases[i].r);
            printf(",\"message\":"); jstr(cases[i].m);
            printf(",\"out\":"); jstr(o); printf("}\n");
            free(o);
        }
    }
    return 0;
}
