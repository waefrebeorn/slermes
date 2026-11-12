/* t_port_connectors_contract.c — oracle harness for resolves and allowed.
 * Emits {"tag":"res",...} and {"tag":"alw",...} lines. */
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

    static const char *const ak[] = {
        "connector", "connector", "connector", "connector",
        "connector", "connector", "connector", "connector",
        "connector", "connector", "connector", "connector",
        "mcp", "mcp", "mcp", "mcp", "mcp",
        "plugin", "plugin", "skill", "skill",
        "connector", "unknown", "connector", NULL,
    };
    static const char *const af[] = {
        "pending", "pending", "pending", "pending",
        "initiated", "initiated", "initiated", "initiated",
        "failed", "failed", "expired", "expired",
        "pending", "initiated", "initiated", "failed", "failed",
        "pending", "initiated", "pending", "initiated",
        "connected", "pending", "pending", NULL,
    };
    static const char *const at[] = {
        "initiated", "failed", "skipped", "connected",
        "connected", "expired", "failed", "skipped",
        "initiated", "skipped", "initiated", "skipped",
        "initiated", "connected", "expired", "initiated", "skipped",
        "initiated", "connected", "failed", "skipped",
        "skipped", "initiated", "initiated", NULL,
    };
    size_t m = sizeof(af) / sizeof(af[0]);
    for (size_t i = 0; i < m; i++) {
        const char *a = ctc_allowed(ak[i], af[i], at[i]);
        printf("{\"tag\":\"alw\",\"case\":%zu,\"kind\":", i);
        jstr(ak[i]);
        printf(",\"current\":");
        jstr(af[i]);
        printf(",\"to\":");
        jstr(at[i]);
        printf(",\"out\":");
        jstr(a);
        printf("}\n");
    }
    /* NULL kind -> no transition permitted */
    printf("{\"tag\":\"alw\",\"case\":%zu,\"kind\":null,\"current\":\"pending\","
           "\"to\":\"initiated\",\"out\":null}\n", m);
    return 0;
}
