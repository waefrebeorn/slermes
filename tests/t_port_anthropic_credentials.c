/*
 * t_port_anthropic_credentials.c — Oracle harness for the classification core
 * of agent/anthropic_credentials.py (src/cli/port_anthropic_credentials.c).
 */
#include "port_anthropic_credentials.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void emit_json_str(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    fputc('"', stdout);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"':  fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        default:
            if (*p < 0x20) printf("\\u%04x", *p);
            else fputc((char)*p, stdout);
        }
    }
    fputc('"', stdout);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    static const char *const tokens[] = {
        NULL, "", "sk-ant-oat01-abc", "sk-ant-api03-abc", "sk-ant-usr-1",
        "sk-ant-svc-1", "sk-ant-admin-1", "sk-ant-future-1", "eyJhbGciOi",
        "cc-abc", "sk-ant", "SK-ANT-OAT", " anth", "x-sk-ant-oat",
    };
    for (size_t i = 0; i < sizeof(tokens) / sizeof(tokens[0]); i++) {
        printf("{\"family\":\"oauth_token\",\"token\":");
        emit_json_str(tokens[i]);
        printf(",\"ret\":%d}\n", anc_is_oauth_token(tokens[i]));
    }

    static const char *const urls[] = {
        NULL, "", "   ", "api.anthropic.com", "https://api.anthropic.com",
        "https://api.anthropic.com/v1", "https://API.Anthropic.COM./v1/",
        "http://api.anthropic.com:443/v1", "https://api.anthropic.com.example/v1",
        "https://proxy.test/api.anthropic.com/v1", "https://user:pw@api.anthropic.com/v1",
        "api.anthropic.com:8443", "https://[::1]:8080/x", "https://other.host/v1",
    };
    for (size_t i = 0; i < sizeof(urls) / sizeof(urls[0]); i++) {
        char *h = anc_base_url_hostname(urls[i]);
        printf("{\"family\":\"host\",\"url\":");
        emit_json_str(urls[i]);
        printf(",\"host\":");
        emit_json_str(h);
        printf(",\"native\":%d}\n", anc_is_native_host(urls[i]));
        free(h);
    }

    {
        struct { const char *url, *cred, *prov; } cases[] = {
            { "https://api.anthropic.com", "sk-ant-oat01-x", NULL },
            { "https://api.anthropic.com", "sk-ant-api03-x", NULL },
            { "https://third.party/v1", "sk-ant-oat01-x", "anthropic" },
            { "https://third.party/v1", "sk-ant-oat01-x", " openrouter " },
            { "https://api.anthropic.com.example/v1", "sk-ant-oat01-x", NULL },
            { "", "eyJabc", NULL },
            { NULL, "cc-token", "Anthropic" },
            { "https://api.anthropic.com", "", NULL },
        };
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            printf("{\"family\":\"route\",\"url\":");
            emit_json_str(cases[i].url);
            printf(",\"cred\":");
            emit_json_str(cases[i].cred);
            printf(",\"provider\":");
            emit_json_str(cases[i].prov);
            printf(",\"ret\":%d}\n",
                   anc_route_is_oauth(cases[i].url, cases[i].cred, cases[i].prov));
        }
    }

    {
        struct { const char *json; double now; const char *name; } cases[] = {
            { "{\"accessToken\":\"a\",\"expiresAt\":1000000}", 900000, "valid" },
            { "{\"accessToken\":\"a\",\"expiresAt\":1000000}", 940001, "within_buffer" },
            { "{\"accessToken\":\"a\",\"expiresAt\":1000000}", 1000000, "expired" },
            { "{\"accessToken\":\"a\"}", 1, "managed_key_present" },
            { "{\"accessToken\":\"\"}", 1, "managed_key_empty" },
            { "{}", 1, "no_token_no_expiry" },
            { "{\"expiresAt\":0,\"accessToken\":\"a\"}", 5, "zero_expiry" },
        };
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            printf("{\"family\":\"token_valid\",\"case\":\"%s\",\"json\":",
                   cases[i].name);
            emit_json_str(cases[i].json);
            printf(",\"now\":%.17g,\"ret\":%d}\n", cases[i].now,
                   anc_claude_token_valid(cases[i].json, cases[i].now));
        }
    }

    {
        struct { const char *dir, *home, *name; } cases[] = {
            { NULL, "/home/u", "default" },
            { "", "/home/u", "empty_env" },
            { "   ", "/home/u", "blank_env" },
            { "/custom/cfg", "/home/u", "absolute" },
            { "~/cfg", "/home/u", "tilde" },
            { "~", "/home/u", "bare_tilde" },
            { "rel/cfg", "/home/u", "relative" },
        };
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            char *p = anc_credentials_path(cases[i].dir, cases[i].home);
            printf("{\"family\":\"creds_path\",\"case\":\"%s\",\"out\":", cases[i].name);
            emit_json_str(p);
            printf("}\n");
            free(p);
        }
    }
    return 0;
}
