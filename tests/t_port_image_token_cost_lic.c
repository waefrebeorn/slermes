/* t_port_image_token_cost_lic.c — oracle harness for learned_image_token_cost.
 * Emits {"tag","model","base_url","table","out"} (out = decimal cost). */
#include "port_image_token_cost.h"

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
    struct { const char *m, *b, *t; } cases[] = {
        {"gpt-4o", "https://api.openai.com/v1",
         "{\"gpt-4o@api.openai.com\":2100,\"claude@api.anthropic.com\":4000}"},
        {"claude", "https://api.anthropic.com/v1",
         "{\"gpt-4o@api.openai.com\":2100,\"claude@api.anthropic.com\":4000}"},
        {"unknown-model", "https://api.example.com/v1", NULL},
        {"", "", NULL},
        {"gpt", "https://api.openai.com.example/v1", NULL},
        {"claude", "wss://x.ai", "{\"claude@x.ai\":900}"},
        {"svc", "api.openai.com:8080", "{\"svc@api.openai.com\":777}"},
        {"Up", "API.EXAMPLE.COM", "{\"up@api.example.com\":333}"},
        {"t", "https://user:pass@api.example.com/path", "{\"t@api.example.com\":456}"},
    };
    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
        char *o = pic_learned_image_token_cost(cases[i].m, cases[i].b, cases[i].t);
        printf("{\"tag\":\"lic\",\"case\":%zu,\"model\":", i);
        jstr(cases[i].m);
        printf(",\"base_url\":"); jstr(cases[i].b);
        printf(",\"table\":"); jstr(cases[i].t);
        printf(",\"out\":%s}\n", o ? o : "null");
        free(o);
    }
    return 0;
}
