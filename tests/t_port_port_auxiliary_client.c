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

    /* ── catalog selection ── */
    {
        static const char *const CATS[][8] = {
            {"gpt-5.4-nano", "gpt-5.4-mini", "claude-haiku-4.5", "grok-4.6"},
            {"gpt-5-mini", "gpt-5.10-mini", "gpt-5.9-mini"},
            {"gemini-3.6-flash", "gemini-2.5-flash", "gemini-3-flash-lite"},
            {"claude-3-5-sonnet", "claude-haiku-4.5", "claude-opus-5"},
            {"llama-3-1-8b", "mistral-small"},
            {"gpt-5.4-nano-thinking", "gpt-5.4-nano"},
            {"gpt-5.4-nano:free", "gpt-5.4-nano"},
            {"o1-mini", "o3-mini", "gpt-5.4-mini"},
            {"text-embedding-3-small", "gpt-5.4-mini"},
            {"gpt-5.4-turbo-vl", "gpt-5.4-mini"},
            {"nvidia/nemotron-3-nano:30b", "gpt-5.4-nano"},
            {"gemini-flash-latest", "gemini-3.6-flash"},
            {"gpt-nano-latest", "gpt-5.4-nano"},
            {"haiku", "gpt-5.4-mini"},
            {"my-haiku-model", "gpt-5.4-mini"},
            {"deepseek-r1", "gpt-5.4-mini"},
        };
        static const int CATN[] = {4, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2};
        for (size_t c = 0; c < sizeof(CATN) / sizeof(CATN[0]); c++) {
            char picked[512];
            ac_fast_model_select(CATS[c], CATN[c], picked, sizeof(picked));
            printf("{\"fn\":\"fastselect\",\"ids\":[");
            for (int k = 0; k < CATN[c]; k++) {
                if (k) putchar(',');
                jstr(CATS[c][k]);
            }
            printf("],\"out\":");
            jstr(picked);
            fputs("}\n", stdout);
        }
    }

    /* ── aux-model fallback table ── */
    {
        static const char *const PROVS[] = {
            "gemini", "zai", "kimi-coding", "stepfun", "kimi-coding-cn", "gmi",
            "anthropic", "ai-gateway", "opencode-zen", "opencode-go", "kilocode",
            "ollama-cloud", "tencent-tokenhub", "tencent-tokenplan",
            "deepinfra", "nous", "openai", "", "GEMINI",
        };
        for (size_t i = 0; i < sizeof(PROVS) / sizeof(PROVS[0]); i++) {
            char out[256];
            ac_aux_model_fallback(PROVS[i], out, sizeof(out));
            printf("{\"fn\":\"auxfallback\",\"arg\":");
            jstr(PROVS[i]);
            printf(",\"out\":");
            jstr(out);
            fputs("}\n", stdout);
        }
    }

    /* ── fast-model task set ── */
    {
        static const char *const TASKS[] = {
            "title_generation", "title-generation", "", "summarize", ""
        };
        for (size_t i = 0; i < sizeof(TASKS) / sizeof(TASKS[0]); i++) {
            printf("{\"fn\":\"fasttask\",\"arg\":");
            jstr(TASKS[i] ? TASKS[i] : "");
            printf(",\"out\":%d}\n", ac_task_can_prefer_fast(TASKS[i]));
        }
    }

    /* ── provider vision static table ── */
    {
        static const char *const PROVS[] = {
            "xiaomi", "zai", "gemini", "openai", "", "XIAOMI",
        };
        for (size_t i = 0; i < sizeof(PROVS) / sizeof(PROVS[0]); i++) {
            char out[256];
            int hit = ac_provider_vision_static(PROVS[i], out, sizeof(out));
            printf("{\"fn\":\"visionstatic\",\"arg\":");
            jstr(PROVS[i]);
            printf(",\"hit\":%d,\"out\":", hit);
            if (hit) jstr(out); else fputs("null", stdout);
            fputs("}\n", stdout);
        }
    }

    /* ── free model / contains_any ── */
    {
        static const char *const MODELS[] = {
            "", "x:free", "x:Free", "stealth/llama", "STEALTH/x", "x:free ",
            " x:free", "x:freex", "x:paid", "stealth", "/", "stealth/x:free",
        };
        for (size_t i = 0; i < sizeof(MODELS) / sizeof(MODELS[0]); i++) {
            printf("{\"fn\":\"freemodel\",\"arg\":");
            jstr(MODELS[i]);
            printf(",\"out\":%d}\n", ac_is_free_model(MODELS[i]));
        }

        static const char *const TEXTS[] = {
            "", "Budget limit exceeded", "nothing here", "RESOURCE_EXHAUSTED",
            "resource-exhausted", "quota_exceeded", "credits",
        };
        static const char *const NEEDLES1[] = {"budget limit", "key limit"};
        static const char *const NEEDLES2[] = {"resource exhausted",
                                              "resource_exhausted",
                                              "resource-exhausted"};
        static const char *const NEEDLES3[] = {"credits", "insufficient funds"};
        static const char *const NEEDLES4[] = {};
        for (size_t i = 0; i < sizeof(TEXTS) / sizeof(TEXTS[0]); i++) {
            printf("{\"fn\":\"contains\",\"arg\":");
            jstr(TEXTS[i]);
            printf(",\"set\":1,\"out\":%d}\n",
                   ac_contains_any(TEXTS[i], NEEDLES1, 2));
            printf("{\"fn\":\"contains\",\"arg\":");
            jstr(TEXTS[i]);
            printf(",\"set\":2,\"out\":%d}\n",
                   ac_contains_any(TEXTS[i], NEEDLES2, 3));
            printf("{\"fn\":\"contains\",\"arg\":");
            jstr(TEXTS[i]);
            printf(",\"set\":3,\"out\":%d}\n",
                   ac_contains_any(TEXTS[i], NEEDLES3, 2));
            printf("{\"fn\":\"contains\",\"arg\":");
            jstr(TEXTS[i]);
            printf(",\"set\":4,\"out\":%d}\n",
                   ac_contains_any(TEXTS[i], NEEDLES4, 0));
        }
    }

    /* ── chain labels ── */
    {
        static const char *const PROVS[] = {
            "", "  ", "openrouter", "OpenRouter", "nous", "custom",
            "local/custom", "openai-codex", "codex", "CODEX", "unknown-prov",
            " openai-codex ", "LOCAL/CUSTOM",
        };
        for (size_t i = 0; i < sizeof(PROVS) / sizeof(PROVS[0]); i++) {
            char out[256];
            ac_normalize_chain_label(PROVS[i], out, sizeof(out));
            printf("{\"fn\":\"chainlabel\",\"arg\":");
            jstr(PROVS[i]);
            printf(",\"out\":");
            jstr(out);
            fputs("}\n", stdout);
        }
    }

    /* ── anthropic endpoint detection ── */
    {
        static const char *const URLS[] = {
            "", "   ", "https://api.anthropic.com",
            "https://api.anthropic.com/v1/messages",
            "https://api.kimi.com/coding",
            "https://api.kimi.com/v1", "https://api.kimi.com/coding/v1",
            "https://gateway.example.com/anthropic",
            "https://gateway.example.com/anthropic/v1",
            "https://gateway.example.com/anthropic/",
            "https://gateway.example.com/v1/anthropic",
            "https://gateway.example.com/v1/messages",
            "https://api.anthropic.com.evil.com/v1",
            "https://evil.com/api.anthropic.com",
            "API.ANTHROPIC.COM", "https://minimax.com/anthropic",
        };
        for (size_t i = 0; i < sizeof(URLS) / sizeof(URLS[0]); i++) {
            printf("{\"fn\":\"anthhost\",\"arg\":");
            jstr(URLS[i]);
            printf(",\"out\":%d}\n", ac_is_anthropic_compatible_host(URLS[i]));
            printf("{\"fn\":\"anthmsg\",\"arg\":");
            jstr(URLS[i]);
            printf(",\"out\":%d}\n",
                   ac_endpoint_speaks_anthropic_messages(URLS[i]));
        }
    }
    return 0;
}
