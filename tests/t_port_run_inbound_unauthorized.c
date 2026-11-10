/* t_port_run_inbound_unauthorized.c — oracle harness for pairing_profile_arg.
 * Emits {"tag":"pair","profile":<json|null>,"out":<json string>}. */
#include "port_run_inbound_unauthorized.h"

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
        "work", "default", "", "my-prod", "default2", "a b",
        "default", NULL,
    };
    size_t n = sizeof(profs) / sizeof(profs[0]);
    for (size_t i = 0; i < n; i++) {
        char *out = run_inbound_pairing_profile_arg(profs[i]);
        printf("{\"tag\":\"pair\",\"case\":%zu,\"profile\":", i);
        jstr(profs[i]);
        printf(",\"out\":");
        jstr(out);
        printf("}\n");
        free(out);
    }
    printf("{\"tag\":\"pair\",\"case\":%zu,\"profile\":999,\"out\":", n);
    char *out = run_inbound_pairing_profile_arg(NULL);
    jstr(out);
    printf("}\n");
    free(out);
    return 0;
}
