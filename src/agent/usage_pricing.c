/*
 * usage_pricing.c -- Token cost estimation for Hermes C.
 *
 * Per-model and per-family pricing. Python equivalent: agent/usage_pricing.py
 *
 * MIT License -- WuBu Slermes Project
 */

#include "usage_pricing.h"
#include "hermes_tokenizer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <ctype.h>

/* ================================================================
 *  Per-model pricing entries
 * ================================================================ */

typedef struct {
    const char *provider;
    const char *model_name;    /* substring match (NULL = catch-all for provider) */
    double  input_per_1m;      /* $ per 1M input tokens */
    double  output_per_1m;     /* $ per 1M output tokens */
    double  cache_read_per_1m; /* $ per 1M cache read tokens */
    double  cache_write_per_1m;
} pricing_entry_t;

/* Official docs snapshot pricing for common models.
 * Cache rates: Anthropic = 10% input for read, 125% input for write.
 * OpenAI = 50% input for read. DeepSeek = 10% input for read. */
static const pricing_entry_t PRICING_TABLE[] = {
    /* -- Anthropic Claude 4.x Opus -- */
    {"anthropic","claude-opus-4",          5.00,  25.00,  0.50,   6.25},
    /* -- Anthropic Claude 4.x Sonnet -- */
    {"anthropic","claude-sonnet-4",        3.00,  15.00,  0.30,   3.75},
    /* -- Anthropic Claude 4.x Haiku -- */
    {"anthropic","claude-haiku-4",         1.00,   5.00,  0.10,   1.25},
    /* -- Anthropic Claude 3.5 -- */
    {"anthropic","claude-3.5",             2.00,  10.00,  0.20,   2.50},
    {"anthropic","claude-3",               2.00,  10.00,  0.20,   2.50},
    /* -- Anthropic catch-all -- */
    {"anthropic",NULL,                     3.00,  15.00,  0.30,   3.75},

    /* -- OpenAI GPT-4o -- */
    {"openai","gpt-4o",                   10.00,  30.00,  5.00,   0.0},
    {"openai","gpt-4o-mini",              0.15,   0.60,  0.075,  0.0},
    {"openai","gpt-4-turbo",             10.00,  30.00,  5.00,   0.0},
    {"openai","gpt-4",                   30.00,  60.00, 15.00,   0.0},
    {"openai","gpt-3.5-turbo",            0.50,   1.50,  0.25,   0.0},
    {"openai","o1",                      15.00,  60.00,  7.50,   0.0},
    {"openai","o3",                      10.00,  40.00,  5.00,   0.0},
    /* -- OpenAI catch-all -- */
    {"openai",NULL,                       10.00,  30.00,  5.00,   0.0},

    /* -- DeepSeek -- */
    {"deepseek","deepseek-chat",           0.27,   1.10,  0.027,  0.0},
    {"deepseek","deepseek-reasoner",       0.55,   2.19,  0.055,  0.0},
    {"deepseek","deepseek-r1",             0.55,   2.19,  0.055,  0.0},
    /* -- DeepSeek catch-all -- */
    {"deepseek",NULL,                      0.27,   1.10,  0.027,  0.0},

    /* -- xAI Grok -- */
    {"xai","grok-3",                      5.00,  15.00,  2.50,   0.0},
    {"xai","grok-2",                      2.00,  10.00,  1.00,   0.0},
    {"xai",NULL,                          5.00,  15.00,  2.50,   0.0},

    /* -- Google Gemini -- */
    {"google","gemini-2.0",               1.25,   5.00,  0.0,    0.0},
    {"google","gemini-1.5-pro",           1.25,   5.00,  0.0,    0.0},
    {"google","gemini-1.5-flash",         0.075,  0.30,  0.0,    0.0},
    {"google","gemini-2.0-flash",         0.10,   0.40,  0.0,    0.0},
    {"google",NULL,                       1.25,   5.00,  0.0,    0.0},

    /* -- Azure OpenAI -- */
    {"azure","gpt-4o",                   10.00,  30.00,  5.00,   0.0},
    {"azure",NULL,                       10.00,  30.00,  5.00,   0.0},

    /* -- Amazon Bedrock -- */
    {"bedrock","claude",                   3.00,  15.00,  0.30,   3.75},
    {"bedrock",NULL,                       3.00,  15.00,  0.30,   3.75},
};

static const int PRICING_TABLE_COUNT = sizeof(PRICING_TABLE) / sizeof(PRICING_TABLE[0]);

/* ================================================================
 *  Provider + model extraction
 * ================================================================ */

/* Extract provider from "provider/model" format, or empty string if none.
 * Port of Python agent/usage_pricing.py:_normalize_anthropic_model_name */
static void extract_provider(const char *model, char *provider, size_t psize) {
    provider[0] = '\0';
    if (!model) return;
    const char *slash = strchr(model, '/');
    if (slash) {
        size_t len = (size_t)(slash - model);
        if (len >= psize) len = psize - 1;
        memcpy(provider, model, len);
        provider[len] = '\0';
    }
}

/* Get the model name part (after "/" if any, otherwise the whole string). */
static const char *model_name_part(const char *model) {
    if (!model) return "";
    const char *slash = strchr(model, '/');
    return slash ? slash + 1 : model;
}

/* ================================================================
 *  Lookup
 * ================================================================ */

/* Find the best pricing entry for a provider+model combination.
 * Port of Python agent/usage_pricing.py:_lookup_official_docs_pricing
 * Port of Python agent/usage_pricing.py:get_pricing_entry
 * First tries exact model match, then provider catch-all, then returns family from tokenizer. */
static bool lookup_pricing(const char *provider, const char *model,
                           double *input, double *output,
                           double *cache_read, double *cache_write,
                           char *source, size_t source_size) {
    /* Try per-model exact/substring match first */
    int provider_catchall = -1;
    for (int i = 0; i < PRICING_TABLE_COUNT; i++) {
        if (strcasecmp(PRICING_TABLE[i].provider, provider) != 0)
            continue;
        if (PRICING_TABLE[i].model_name == NULL) {
            if (provider_catchall < 0)
                provider_catchall = i;
            continue;
        }
        /* Substring match */
        if (strstr(model, PRICING_TABLE[i].model_name)) {
            *input = PRICING_TABLE[i].input_per_1m;
            *output = PRICING_TABLE[i].output_per_1m;
            *cache_read = PRICING_TABLE[i].cache_read_per_1m;
            *cache_write = PRICING_TABLE[i].cache_write_per_1m;
            snprintf(source, source_size, "docs_snapshot");
            return true;
        }
    }

    /* Provider catch-all */
    if (provider_catchall >= 0) {
        const pricing_entry_t *p = &PRICING_TABLE[provider_catchall];
        *input = p->input_per_1m;
        *output = p->output_per_1m;
        *cache_read = p->cache_read_per_1m;
        *cache_write = p->cache_write_per_1m;
        snprintf(source, source_size, "docs_snapshot");
        return true;
    }

    /* Fall back to family-level rates from tokenizer */
    token_family_t family = hermes_token_family_from_model(model);
    token_cost_rates_t rates = hermes_token_cost_rates(family);
    if (rates.input_per_1m > 0 || rates.output_per_1m > 0) {
        *input = rates.input_per_1m;
        *output = rates.output_per_1m;
        *cache_read = rates.input_per_1m * 0.5;   /* 50% of input for cache read */
        *cache_write = 0.0;                         /* unknown */
        snprintf(source, source_size, "family_rates");
        return true;
    }

    return false; /* unknown pricing */
}

/* ================================================================
 *  Public API
 * ================================================================ */

/* Port of Python agent/usage_pricing.py:estimate_usage_cost
 * Port of Python agent/usage_pricing.py:_per_token_to_per_million (inline)
 * Port of Python agent/usage_pricing.py:_to_decimal (inline via double math)
 * Port of Python agent/usage_pricing.py:_to_int (inline via int cast) */
usage_cost_t usage_pricing_estimate(const char *model, const usage_counts_t *usage) {
    usage_cost_t cost;
    memset(&cost, 0, sizeof(cost));

    if (!model) model = "unknown";
    if (!usage) {
        cost.status = COST_STATUS_UNKNOWN;
        cost.has_pricing = false;
        return cost;
    }

    char provider[PRICING_PROVIDER_MAX] = "";
    extract_provider(model, provider, sizeof(provider));
    const char *mname = model_name_part(model);

    double input_rate = 0, output_rate = 0, cache_read_rate = 0, cache_write_rate = 0;
    char source[PRICING_SOURCE_MAX] = "none";

    bool found = lookup_pricing(provider, mname,
                                &input_rate, &output_rate,
                                &cache_read_rate, &cache_write_rate,
                                source, sizeof(source));

    if (!found) {
        cost.status = COST_STATUS_UNKNOWN;
        cost.has_pricing = false;
        snprintf(cost.label, sizeof(cost.label), "%s: unknown pricing", model);
        return cost;
    }

    cost.has_pricing = true;
    cost.status = COST_STATUS_ESTIMATED;
    snprintf(cost.source, sizeof(cost.source), "%s", source);

    /* Compute costs: tokens / 1M * rate */
    cost.input_cost = (double)usage->input_tokens / 1000000.0 * input_rate;
    cost.output_cost = (double)usage->output_tokens / 1000000.0 * output_rate;
    cost.cache_read_cost = (double)usage->cache_read_tokens / 1000000.0 * cache_read_rate;
    cost.cache_write_cost = (double)usage->cache_write_tokens / 1000000.0 * cache_write_rate;
    cost.total_cost = cost.input_cost + cost.output_cost
                    + cost.cache_read_cost + cost.cache_write_cost;

    snprintf(cost.label, sizeof(cost.label),
             "$%.2f/$%.2f per 1M tokens (in/out)",
             input_rate, output_rate);

    if (cache_read_rate > 0 || cache_write_rate > 0) {
        size_t len = strlen(cost.label);
        snprintf(cost.label + len, sizeof(cost.label) - len,
                 "; cache $%.2f/$%.2f", cache_read_rate, cache_write_rate);
    }

    return cost;
}

/* Port of Python agent/usage_pricing.py:has_known_pricing(). */
bool usage_pricing_known(const char *model) {
    if (!model) return false;
    char provider[PRICING_PROVIDER_MAX] = "";
    extract_provider(model, provider, sizeof(provider));
    const char *mname = model_name_part(model);

    double input_rate, output_rate, cache_read_rate, cache_write_rate;
    char source[PRICING_SOURCE_MAX];

    return lookup_pricing(provider, mname,
                          &input_rate, &output_rate,
                          &cache_read_rate, &cache_write_rate,
                          source, sizeof(source));
}

/* Port of Python agent/usage_pricing.py:format_token_count_compact */
const char *usage_pricing_format_cost(double usd) {
    static char buf[32];
    if (usd < 0.0001) {
        snprintf(buf, sizeof(buf), "$0.00");
    } else if (usd < 0.01) {
        snprintf(buf, sizeof(buf), "$%.4f", usd);
    } else if (usd < 1.0) {
        snprintf(buf, sizeof(buf), "$%.2f", usd);
    } else if (usd < 100.0) {
        snprintf(buf, sizeof(buf), "$%.2f", usd);
    } else {
        snprintf(buf, sizeof(buf), "$%.0f", usd);
    }
    return buf;
}

/* Port of Python agent/usage_pricing.py:format_duration_compact */
const char *usage_pricing_format_duration(double seconds) {
    static char buf[24];
    if (seconds < 0) seconds = 0;

    if (seconds < 60.0) {
        snprintf(buf, sizeof(buf), "%ds", (int)seconds);
    } else if (seconds < 3600.0) {
        snprintf(buf, sizeof(buf), "%dm", (int)(seconds / 60.0));
    } else if (seconds < 86400.0) {
        snprintf(buf, sizeof(buf), "%.1fh", seconds / 3600.0);
    } else {
        snprintf(buf, sizeof(buf), "%.1fd", seconds / 86400.0);
    }
    return buf;
}

/* Port of Python agent/usage_pricing.py:prompt_tokens
 * Port of Python agent/usage_pricing.py:total_tokens */
int usage_pricing_prompt_tokens(const usage_counts_t *usage)
{
    return usage ? usage->input_tokens : 0;
}

/* Port of Python agent/usage_pricing.py:total_tokens() */
int usage_pricing_total_tokens(const usage_counts_t *usage)
{
    if (!usage) return 0;
    return usage->input_tokens + usage->output_tokens
         + usage->cache_read_tokens + usage->cache_write_tokens;
}

/* AG26: Port of Python agent/usage_pricing.py:_normalize(). */
usage_counts_t normalize(const usage_counts_t *usage)
{
    usage_counts_t result;
    if (usage) {
        result = *usage;
    } else {
        memset(&result, 0, sizeof(result));
    }
    return result;
}

/* ================================================================
 *  Billing route resolution (port of Python resolves)
 * ================================================================ */

typedef enum {
    BILLING_DOCS_SNAPSHOT,   /* official pricing table lookup */
    BILLING_MODELS_API,      /* API-fetched model metadata (OpenRouter) */
    BILLING_SUB_INCLUDED,    /* subscription-included (OpenAI Codex) */
    BILLING_UNKNOWN,
} billing_mode_t;

typedef struct {
    char  provider[PRICING_PROVIDER_MAX];
    char  model_name[PRICING_MODEL_NAME_MAX];
    char  base_url[256];
    billing_mode_t mode;
} billing_route_t;

/* Port of Python agent/usage_pricing.py:resolve_billing_route
 * Determines billing provider & mode from model name + optional provider/base_url. */
static __attribute__((unused)) billing_route_t resolve_billing_route(const char *model_name,
                                              const char *provider,
                                              const char *base_url)
{
    billing_route_t route;
    memset(&route, 0, sizeof(route));

    /* Normalise inputs */
    char pbuf[PRICING_PROVIDER_MAX] = "";
    char bbuf[256] = "";
    char mbuf[PRICING_MODEL_NAME_MAX] = "";

    if (provider) {
        size_t i = 0;
        while (provider[i] && i < sizeof(pbuf) - 1) {
            pbuf[i] = (char)tolower((unsigned char)provider[i]);
            i++;
        }
        pbuf[i] = '\0';
        /* strip */
        while (i > 0 && pbuf[i-1] == ' ') pbuf[--i] = '\0';
    }
    if (base_url) {
        size_t i = 0;
        while (base_url[i] && i < sizeof(bbuf) - 1) {
            bbuf[i] = (char)tolower((unsigned char)base_url[i]);
            i++;
        }
        bbuf[i] = '\0';
    }
    if (model_name) {
        size_t i = 0;
        while (model_name[i] && i < sizeof(mbuf) - 1) {
            mbuf[i] = (char)tolower((unsigned char)model_name[i]);
            i++;
        }
        mbuf[i] = '\0';
        while (i > 0 && mbuf[i-1] == ' ') mbuf[--i] = '\0';
    }

    /* Infer provider from model "provider/model" format */
    if (!pbuf[0]) {
        const char *slash = strchr(mbuf, '/');
        if (slash) {
            size_t plen = (size_t)(slash - mbuf);
            if (plen >= sizeof(pbuf)) plen = sizeof(pbuf) - 1;
            memcpy(pbuf, mbuf, plen);
            pbuf[plen] = '\0';
            /* Strip inferred provider from model */
            size_t rest = strlen(slash + 1);
            if (rest >= sizeof(mbuf)) rest = sizeof(mbuf) - 1;
            memmove(mbuf, slash + 1, rest + 1);
        }
    }

    /* Resolve billing mode */
    snprintf(route.provider, sizeof(route.provider), "%s", pbuf);
    snprintf(route.model_name, sizeof(route.model_name), "%s", mbuf);
    snprintf(route.base_url, sizeof(route.base_url), "%s", bbuf);

    if (strcmp(pbuf, "openai-codex") == 0) {
        route.mode = BILLING_SUB_INCLUDED;
    } else if (strcmp(pbuf, "openrouter") == 0 ||
               strstr(bbuf, "openrouter.ai")) {
        route.mode = BILLING_MODELS_API;
    } else if (strcmp(pbuf, "anthropic") == 0 ||
               strcmp(pbuf, "openai") == 0 ||
               strcmp(pbuf, "minimax") == 0 ||
               strcmp(pbuf, "minimax-cn") == 0) {
        route.mode = BILLING_DOCS_SNAPSHOT;
    } else if (strcmp(pbuf, "custom") == 0 ||
               strcmp(pbuf, "local") == 0 ||
               strstr(bbuf, "localhost")) {
        route.mode = BILLING_UNKNOWN;
    } else if (pbuf[0]) {
        /* Unknown provider: try docs snapshot */
        route.mode = BILLING_DOCS_SNAPSHOT;
    } else {
        route.mode = BILLING_UNKNOWN;
    }

    return route;
}

/* Port of Python agent/usage_pricing.py:_pricing_entry_from_metadata
 * Look up model pricing. In C, uses the static PRICING_TABLE (equivalent to
 * the Python docs snapshot metadata dict). */
static bool pricing_entry_from_metadata(const char *model_id,
                                         double *input_per_1m,
                                         double *output_per_1m,
                                         double *cache_read_per_1m,
                                         double *cache_write_per_1m)
{
    if (!model_id || !model_id[0]) return false;

    /* Try exact match first */
    for (int i = 0; i < PRICING_TABLE_COUNT; i++) {
        if (PRICING_TABLE[i].model_name &&
            strcmp(model_id, PRICING_TABLE[i].model_name) == 0) {
            *input_per_1m = PRICING_TABLE[i].input_per_1m;
            *output_per_1m = PRICING_TABLE[i].output_per_1m;
            *cache_read_per_1m = PRICING_TABLE[i].cache_read_per_1m;
            *cache_write_per_1m = PRICING_TABLE[i].cache_write_per_1m;
            return true;
        }
    }

    /* Try substring match */
    for (int i = 0; i < PRICING_TABLE_COUNT; i++) {
        if (PRICING_TABLE[i].model_name &&
            strstr(model_id, PRICING_TABLE[i].model_name)) {
            *input_per_1m = PRICING_TABLE[i].input_per_1m;
            *output_per_1m = PRICING_TABLE[i].output_per_1m;
            *cache_read_per_1m = PRICING_TABLE[i].cache_read_per_1m;
            *cache_write_per_1m = PRICING_TABLE[i].cache_write_per_1m;
            return true;
        }
    }

    return false;
}

/* Port of Python agent/usage_pricing.py:_openrouter_pricing_entry
 * Attempt to find pricing specifically via OpenRouter-style metadata lookup.
 * In C, falls through to the static pricing table. */
static __attribute__((unused)) bool openrouter_pricing_entry(const char *model_id,
                                                              double *input_per_1m,
                                                              double *output_per_1m,
                                                              double *cache_read_per_1m,
                                                              double *cache_write_per_1m)
{
    return pricing_entry_from_metadata(model_id, input_per_1m, output_per_1m,
                                       cache_read_per_1m, cache_write_per_1m);
}
