/* t_port_port_auxiliary_client.c -- harness for the auxiliary_client.py port.
 *
 * Emits one JSON object per case; oracle_auxiliary_client.py diffs it against
 * the real Python. Harness stays dumb: C result only.
 */
#include "port_auxiliary_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void jstr(const char *s)
{
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++) {
        switch (*p) {
        case '"': fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\r': fputs("\\r", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        default:
            if (*p < 0x20) printf("\\u%04x", *p);
            else putchar(*p);
        }
    }
    putchar('"');
}

static const char *const MODELS[] = {
    "", "  ", "gpt-5.4", "vendors/gpt-5.4", "openrouter/gpt-5.4-codex",
    "GPT-5.5-Pro", "kimi-k2", "moonshotai/kimi-k2", "kimi", "KIMI-",
    "trinity-large-thinking", "arcee/trinity-large-thinking",
    "gpt-5.3-codex-spark", "gpt-5.4-nano", "gpt-6-sol", "gpt-6.1-sol-terra",
    "gpt-6-luna", "gpt-daybreak-blue-latest", "gpt-6-astra", "gpt-6-astra-900k",
    "gpt-5.5-900k", "gpt-5.5", "o1-preview", "o3-mini", "o4-mini",
    "gpt-5-chat", "gpt-5-chat-latest", "gpt-3.5-mini", "claude-haiku-4.5",
    "gemini-3.6-flash", "glm-5.3-flash", "qwen3.5-397b-wafer",
    "a.b.c", "x/y/z", "trailing/", "/leading",
};

static const char *const PROVIDERS[] = {
    "", "openai-codex", "OpenAI-Codex ", " openai-codex", "codex",
    "openrouter", "auto", "kimi",
};

static const char *const RECENCY[] = {
    "gpt-3.5-mini", "gpt-5.4-mini", "gpt-5.10-mini", "gpt-5.9-mini",
    "claude-3-5-sonnet", "claude-3.5-sonnet", "claude-3-7-sonnet",
    "gemini-2.5-pro", "gemini-2.10-pro", "qwen3.5-397b-wafer",
    "v1.2.10", "v1.2.9", "a10b2", "GPT-5.4-NANO",
};

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* ac_bare_model */
    for (size_t i = 0; i < sizeof(MODELS) / sizeof(MODELS[0]); i++) {
        char *b = ac_bare_model(MODELS[i]);
        printf("{\"fn\":\"bare\",\"arg\":");
        jstr(MODELS[i]);
        printf(",\"out\":");
        jstr(b ? b : "");
        fputs("}\n", stdout);
        free(b);
    }

    /* predicates over (model, provider) */
    for (size_t i = 0; i < sizeof(MODELS) / sizeof(MODELS[0]); i++) {
        for (size_t p = 0; p < sizeof(PROVIDERS) / sizeof(PROVIDERS[0]); p++) {
            printf("{\"fn\":\"kimi\",\"arg\":");
            jstr(MODELS[i]);
            printf(",\"prov\":");
            jstr(PROVIDERS[p]);
            printf(",\"out\":%d}\n", ac_is_kimi_model(MODELS[i]));

            printf("{\"fn\":\"trinity\",\"arg\":");
            jstr(MODELS[i]);
            printf(",\"prov\":");
            jstr(PROVIDERS[p]);
            printf(",\"out\":%d}\n", ac_is_arcee_trinity_thinking(MODELS[i]));

            printf("{\"fn\":\"codexbare\",\"arg\":");
            jstr(MODELS[i]);
            printf(",\"prov\":");
            jstr(PROVIDERS[p]);
            printf(",\"out\":");
            {
                char *cb = ac_codex_route_bare_model(MODELS[i], PROVIDERS[p]);
                /* Python returns None here, so emit JSON null rather than "" --
                 * the oracle compares types as well as values. */
                if (cb) { jstr(cb); free(cb); }
                else fputs("null", stdout);
            }
            fputs("}\n", stdout);

            printf("{\"fn\":\"spark\",\"arg\":");
            jstr(MODELS[i]);
            printf(",\"prov\":");
            jstr(PROVIDERS[p]);
            printf(",\"out\":%d}\n", ac_is_codex_spark(MODELS[i], PROVIDERS[p]));

            printf("{\"fn\":\"gpt5455\",\"arg\":");
            jstr(MODELS[i]);
            printf(",\"prov\":");
            jstr(PROVIDERS[p]);
            printf(",\"out\":%d}\n", ac_is_codex_gpt54_or_gpt55(MODELS[i], PROVIDERS[p]));

            for (int allow = 0; allow <= 1; allow++) {
                double th = ac_compression_threshold_for_model(MODELS[i], PROVIDERS[p], allow);
                printf("{\"fn\":\"threshold\",\"arg\":");
                jstr(MODELS[i]);
                printf(",\"prov\":");
                jstr(PROVIDERS[p]);
                printf(",\"allow\":%d,\"out\":%.17g}\n", allow, th);
            }
        }
    }

    /* provider-independent predicates */
    for (size_t i = 0; i < sizeof(MODELS) / sizeof(MODELS[0]); i++) {
        printf("{\"fn\":\"tempdefault\",\"arg\":");
        jstr(MODELS[i]);
        printf(",\"out\":%d}\n", ac_is_openai_default_temperature_only(MODELS[i]));
    }

    /* recency keys */
    for (size_t i = 0; i < sizeof(RECENCY) / sizeof(RECENCY[0]); i++) {
        char key[512];
        ac_model_recency_key(RECENCY[i], key, sizeof(key));
        printf("{\"fn\":\"recency\",\"arg\":");
        jstr(RECENCY[i]);
        printf(",\"out\":");
        jstr(key);
        fputs("}\n", stdout);
    }

    /* ── base_url_hostname / host_matches / nvidia ── */
    {
        static const char *const URLS[] = {
            "", "  ", "https://integrate.api.nvidia.com/v1",
            "http://integrate.api.nvidia.com", "integrate.api.nvidia.com",
            "https://sub.integrate.api.nvidia.com/v1",
            "https://evil.com/moonshot.ai/v1", "https://moonshot.ai.evil.com",
            "https://proxy.test/api.openai.com/v1", "https://api.openai.com.example/v1",
            "https://user:pw@Host.EXAMPLE.com:8080/v1", "host.example.com.",
            "https://host.example.com./v1", "//bare.example.com/x",
            "https://[::1]:8080/v1", "ftp://files.example.com/x",
        };
        static const char *const DOMAINS[] = {
            "integrate.api.nvidia.com", "moonshot.ai", "api.openai.com",
            "example.com", "EXAMPLE.COM.", "", "host.example.com",
        };
        for (size_t i = 0; i < sizeof(URLS) / sizeof(URLS[0]); i++) {
            char *h = ac_base_url_hostname(URLS[i]);
            printf("{\"fn\":\"hostname\",\"arg\":");
            jstr(URLS[i]);
            printf(",\"out\":");
            if (h) { jstr(h); free(h); } else fputs("null", stdout);
            fputs("}\n", stdout);

            printf("{\"fn\":\"nvidia\",\"arg\":");
            jstr(URLS[i]);
            printf(",\"out\":%d}\n", ac_nvidia_nim_host(URLS[i]));

            for (size_t d = 0; d < sizeof(DOMAINS) / sizeof(DOMAINS[0]); d++) {
                printf("{\"fn\":\"hostmatch\",\"arg\":");
                jstr(URLS[i]);
                printf(",\"domain\":");
                jstr(DOMAINS[d]);
                printf(",\"out\":%d}\n",
                       ac_base_url_host_matches(URLS[i], DOMAINS[d]));
            }
        }
    }

    /* ── _anthropic_event_has_content ── */
    {
        static const char *const EVENTS[] = {
            "{}", "null", "[]",
            "{\"type\":\"content_block_delta\",\"delta\":{\"text\":\"hi\"}}",
            "{\"type\":\"content_block_delta\",\"delta\":{\"text\":\"\"}}",
            "{\"type\":\"content_block_delta\",\"delta\":{\"thinking\":\"t\"}}",
            "{\"type\":\"content_block_delta\",\"delta\":{\"partial_json\":\"{}\"}}",
            "{\"type\":\"content_block_delta\",\"delta\":{\"signature\":\"s\"}}",
            "{\"type\":\"content_block_delta\",\"delta\":{\"citation\":{\"x\":1}}}",
            "{\"type\":\"content_block_delta\",\"delta\":null}",
            "{\"type\":\"content_block_delta\"}",
            "{\"type\":\"content_block_start\",\"content_block\":{\"type\":\"tool_use\",\"id\":\"t1\",\"name\":\"n\"}}",
            "{\"type\":\"content_block_start\",\"content_block\":{\"type\":\"tool_use\",\"id\":\"\",\"name\":\"\"}}",
            "{\"type\":\"content_block_start\",\"content_block\":{\"type\":\"tool_use\",\"id\":\"t1\"}}",
            "{\"type\":\"content_block_start\",\"content_block\":{\"type\":\"text\",\"id\":\"t1\"}}",
            "{\"type\":\"content_block_stop\"}",
            "{\"type\":\"message_start\"}",
            "{\"delta\":{\"text\":\"orphan\"}}",
        };
        for (size_t i = 0; i < sizeof(EVENTS) / sizeof(EVENTS[0]); i++) {
            json_t *ev = json_parse(EVENTS[i], NULL);
            printf("{\"fn\":\"event\",\"arg\":");
            jstr(EVENTS[i]);
            printf(",\"out\":%d}\n",
                   ev ? ac_anthropic_event_has_content(ev) : -1);
            json_free(ev);
        }
    }

    /* ── _ai_gateway_headers ── */
    {
        static const char *const VERSIONS[] = {"1.2.3", "0.15.1", "", "2.0.0-rc1"};
        for (size_t i = 0; i < sizeof(VERSIONS) / sizeof(VERSIONS[0]); i++) {
            json_t *h = ac_ai_gateway_headers(VERSIONS[i]);
            printf("{\"fn\":\"aigateway\",\"arg\":");
            jstr(VERSIONS[i]);
            printf(",\"out\":");
            if (h) { char *s = json_serialize(h); jstr(s ? s : ""); free(s); json_free(h); }
            else fputs("null", stdout);
            fputs("}\n", stdout);
        }
    }

    /* ── build_or_headers ── */
    {
        static const char *const ENVS[] = {"", "1", "true", "YES", "on", "0", "no", "garbage"};
        static const char *const TTLS[] = {"", "60", "300", "86400", "0", "86401", "abc", "12.5"};
        for (size_t e = 0; e < sizeof(ENVS) / sizeof(ENVS[0]); e++) {
            for (int cc = 0; cc <= 1; cc++) {
                for (size_t t = 0; t < sizeof(TTLS) / sizeof(TTLS[0]); t++) {
                    static const double CFG_TTLS[] = {300.0, 0.0, 86400.0, 90000.0};
                    for (size_t k = 0; k < sizeof(CFG_TTLS) / sizeof(CFG_TTLS[0]); k++) {
                        json_t *h = ac_build_or_headers(ENVS[e], cc, TTLS[t], CFG_TTLS[k]);
                        printf("{\"fn\":\"orheaders\",\"env\":");
                        jstr(ENVS[e]);
                        printf(",\"cfgcache\":%d,\"envttl\":", cc);
                        jstr(TTLS[t]);
                        printf(",\"cfgtll\":%.17g,\"out\":", CFG_TTLS[k]);
                        if (h) { char *s = json_serialize(h); jstr(s ? s : ""); free(s); json_free(h); }
                        else fputs("null", stdout);
                        fputs("}\n", stdout);
                    }
                }
            }
        }
    }
    return 0;
}
