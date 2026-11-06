/* t_port_connectors_contract.c — oracle harness for resolves.
 * Emits {"tag":"res","kind":<json|null>,"state":<json>,"out":bool}. */
#include "port_connectors_contract.h"

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
    static const char *const kinds[] = {
        "connector", "mcp", "plugin", "skill", "connector", "mcp", "plugin",
        "skill", "plugin", "skill", "plugin", "skill", NULL,
    };
    static const char *const states[] = {
        "connected", "connected", "connected", "connected",
        "skipped", "skipped", "skipped", "skipped",
        "failed", "failed", "pending", "initiated",
    };
    size_t n = sizeof(states) / sizeof(states[0]);
    for (size_t i = 0; i < n; i++) {
        printf("{\"tag\":\"res\",\"case\":%zu,\"kind\":", i);
        jstr(kinds[i]);
        printf(",\"state\":");
        jstr(states[i]);
        printf(",\"out\":%d}\n", ctc_resolves(kinds[i], states[i]));
    }
    /* NULL kind -> default base set (only connected/skipped) */
    printf("{\"tag\":\"res\",\"case\":%zu,\"kind\":null,\"state\":\"connected\",\"out\":%d}\n",
           n, ctc_resolves(NULL, "connected"));
    printf("{\"tag\":\"res\",\"case\":%zu,\"kind\":null,\"state\":\"failed\",\"out\":%d}\n",
           n + 1, ctc_resolves(NULL, "failed"));
    return 0;
}
