/* t_port_agent_provider.c — oracle harness for agent/provider_registry.py
 * pure helpers: lower_key and the provider-name mapping half. */
#include "port_agent_provider.h"

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

    /* lower_key: strip + lowercase, incl. NULL (-> "") and mixed case. */
    static const char *const names[] = {
        NULL, "", "OpenAI", " Nous ", "openrouter", "  WECom  ", "Claude",
        "deepseek", " x ",
    };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        char *out = ap_lower_key(names[i]);
        printf("{\"family\":\"lk\",\"case\":\"lk_%zu\",\"in\":", i);
        jstr(names[i]);
        printf(",\"out\":");
        jstr(out);
        printf("}\n");
        free(out);
    }

    /* configured_provider_name: raw <section>.provider -> mapped value. */
    static const char *const provs[] = {
        NULL, "", "nous", "NOUS", "  nous  ", "openai", "anthropic",
        "fal", " Nous", "wecom ",
    };
    for (size_t i = 0; i < sizeof(provs) / sizeof(provs[0]); i++) {
        char *out = ap_configured_provider_name(provs[i]);
        printf("{\"family\":\"cpn\",\"case\":\"cpn_%zu\",\"in\":", i);
        jstr(provs[i]);
        printf(",\"out\":");
        jstr(out);
        printf("}\n");
        free(out);
    }
    return 0;
}
