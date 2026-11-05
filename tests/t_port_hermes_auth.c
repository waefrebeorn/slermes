/* t_port_hermes_auth.c — Oracle harness for the pure auth URL/key predicates. */
#include "port_hermes_auth.h"

#include <stdio.h>

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

    static const char *const locals[] = {
        "http://localhost:5006", "https://127.0.0.1/v1", "http://[::1]",
        "http://0.0.0.0/x", "https://api.actual.inc", "https://example.com",
        "", "http://LOCALHOST.", NULL,
    };
    for (size_t i = 0; i < sizeof(locals) / sizeof(locals[0]); i++) {
        printf("{\"family\":\"local\",\"case\":\"l_%zu\",\"in\":", i);
        jstr(locals[i]);
        printf(",\"ret\":%d}\n", auth_is_actual_local_base_url(locals[i]));
    }

    static const char *const norm[] = {
        "https://api.actual.inc", "https://api.actual.inc/",
        "http://localhost:5006", "https://127.0.0.1",
        "https://api.actual.inc/v1", "  https://api.actual.inc/v1/  ",
        "https://eu.actual.inc", "", "https://example.com/api",
        "https://0.0.0.0/base", NULL,
    };
    for (size_t i = 0; i < sizeof(norm) / sizeof(norm[0]); i++) {
        printf("{\"family\":\"normalize\",\"case\":\"n_%zu\",\"in\":", i);
        jstr(norm[i]);
        printf(",\"out\":");
        jstr(auth_normalize_actual_base_url(norm[i]));
        printf("}\n");
    }

    static const char *const ph[] = {
        "your-api-key-here", "YOUR_KEY_HERE", "sk-xxxxx", "sk-ant-123",
        "sk-abc", "ghp_xxxxxxxx", "hf_yyyy", "982039384", "xxxx", "xx xx",
        "sk-", "", "your_placeholder_here", NULL,
        "sk-xxxX",
    };
    for (size_t i = 0; i < sizeof(ph) / sizeof(ph[0]); i++) {
        printf("{\"family\":\"placeholder\",\"case\":\"ph_%zu\",\"in\":", i);
        jstr(ph[i]);
        printf(",\"ret\":%d}\n", auth_is_placeholder_shape(ph[i]));
    }

    static const struct { const char *prov, *val; } kp[] = {
        { "openrouter", "sk-or-v1-abc" }, { "openrouter", "sk-abc" },
        { "openrouter", "" }, { "anthropic", "sk-ant-123" },
        { "openrouter", "SK-OR-V1-XYZ" }, { "openai", "sk-123" },
    };
    for (size_t i = 0; i < sizeof(kp) / sizeof(kp[0]); i++) {
        printf("{\"family\":\"keyprefix\",\"case\":\"k_%zu\",\"prov\":", i);
        jstr(kp[i].prov);
        printf(",\"val\":");
        jstr(kp[i].val);
        printf(",\"ret\":%d}\n", auth_matches_key_prefix(kp[i].prov, kp[i].val));
    }

    static const char *const ork[] = { "sk-or-v1-abc", " sk-or-v1-xyz ", NULL, "", "sk-abc" };
    for (size_t i = 0; i < sizeof(ork) / sizeof(ork[0]); i++) {
        printf("{\"family\":\"openrouter\",\"case\":\"o_%zu\",\"in\":", i);
        jstr(ork[i]);
        printf(",\"ret\":%d}\n", auth_looks_like_openrouter_key(ork[i]));
    }
    return 0;
}
