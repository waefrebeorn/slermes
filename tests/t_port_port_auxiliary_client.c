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
    return 0;
}
