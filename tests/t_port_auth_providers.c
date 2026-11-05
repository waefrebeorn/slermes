/* t_port_auth_providers.c — oracle harness for hermes_cli/auth_commands.py
 * pure helpers: _provider_base_url and _is_known_provider. */
#include "port_auth_providers.h"

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

    /* _provider_base_url: (provider, registry, configured, custom). */
    static const char *const bp[][4] = {
        {"openrouter", "", "", ""},
        {"custom:my", "", "", " https://custom.io/v1 "},
        {"custom:my", "", "", ""},
        {"anthropic", "", "https://cfg.example", ""},
        {"anthropic", "", "", ""},
        {"deepseek", "https://reg.example", "", ""},
        {"deepseek", "https://reg.example", "  ", ""},
    };
    for (int i = 0; i < 7; i++) {
        char *out = cpa_provider_base_url(bp[i][0], bp[i][1], bp[i][2], bp[i][3]);
        printf("{\"family\":\"burl\",\"case\":%d,\"provider\":", i);
        jstr(bp[i][0]);
        printf(",\"reg\":");
        jstr(bp[i][1]);
        printf(",\"cfg\":");
        jstr(bp[i][2]);
        printf(",\"cust\":");
        jstr(bp[i][3]);
        printf(",\"out\":");
        jstr(out);
        printf("}\n");
        free(out);
    }

    /* _is_known_provider: (provider, in_registry, configured). */
    static const char *const ik[][1] = {{"openai"}, {"openrouter"}, {"custom:foo"},
                                        {"unknown"}, {"customx"}, {"xyz"}, {"unk2"}};
    static const int ikf[][2] = {{1,0},{0,0},{0,0},{0,0},{0,1},{1,0},{0,1}};
    for (int i = 0; i < 7; i++) {
        int out = cpa_is_known_provider(ik[i][0], ikf[i][0], ikf[i][1]);
        printf("{\"family\":\"iknow\",\"case\":%d,\"provider\":", i);
        jstr(ik[i][0]);
        printf(",\"inreg\":%d,\"cfg\":%d,\"out\":%d}\n",
               ikf[i][0], ikf[i][1], out);
    }
    /* _resolve_custom_provider_input: (raw, entries). */
    static const char *const RS[][2] = {
        {"my-provider", "[{\"name\":\"My Provider\",\"provider_key\":\"myprov\",\"pool_key\":\"custom:my-provider\"}]"},
        {"myprov", "[{\"name\":\"My Provider\",\"provider_key\":\"myprov\",\"pool_key\":\"custom:my-provider\"}]"},
        {"custom:foo", "[]"},
        {"", "[]"},
        {"zz-z", "[{\"name\":\"ZZ Z\",\"provider_key\":\"\",\"pool_key\":\"custom:zz-z\"}]"},
        {"none", "[]"},
        {"a b", "[{\"name\":\"A B\",\"provider_key\":\"pk\",\"pool_key\":\"custom:a-b\"}]"},
        {"some-name", "[{\"name\":\"Other\",\"provider_key\":\"other\",\"pool_key\":\"custom:other\"},{\"name\":\"Some Name\",\"provider_key\":\"\",\"pool_key\":\"custom:some-name\"}]"},
        {" my-provider ", "[{\"name\":\"My Provider\",\"provider_key\":\"myprov\",\"pool_key\":\"custom:my-provider\"}]"},
    };
    for (int i = 0; i < 9; i++) {
        char *out = apa_resolve_custom_provider_input(RS[i][0], RS[i][1], "custom:");
        printf("{\"family\":\"resolve\",\"case\":%d,\"raw\":", i);
        jstr(RS[i][0]);
        printf(",\"entries\":%s,\"out\":", RS[i][1]);
        jstr(out);
        printf("}\n");
        free(out);
    }
    return 0;
}
