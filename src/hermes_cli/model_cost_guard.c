#define _POSIX_C_SOURCE 200809L
/*
 * Faithful port of hermes_cli/model_cost_guard.py helpers used by
 * expensive_model_warning. These are pure (no live agent/thread objects):
 *   - _format_money(value)        -> "$%.2f/M" or "unknown" for NULL
 *   - _pricing_from_model_info(m) -> (input, output, "models.dev") or
 *                                    (NULL,NULL,"") when cost data absent
 *
 * In Python ModelInfo carries cost_input/cost_output (float). The C side
 * represents a resolved model as a models.dev JSON entry whose `cost` object
 * holds `input`/`output` numbers (see provider_metadata.c format_cost). We
 * mirror that shape so the helper slots into the existing resolution path.
 */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Format a per-million cost. value==NULL -> "unknown". */
/* PoP: model_cost_guard_format_money @ hermes_cli/model_cost_guard.py:_format_money */
void model_cost_guard_format_money(const double *value, char *out, size_t out_sz)
{
    if (!out) return;
    if (!value) { snprintf(out, out_sz, "unknown"); return; }
    snprintf(out, out_sz, "$%.2f/M", *value);
}

/* Extract (input_cost, output_cost, source) from a model info object.
 * Mirrors Python ModelInfo (flat cost_input/cost_output floats), which is also
 * the shape provider_metadata.c emits for the API response. Returns 1 if cost
 * data present, 0 (and empties the out values) otherwise. */
/* PoP: model_cost_guard_pricing_from_model_info @ hermes_cli/model_cost_guard.py:_pricing_from_model_info */
int model_cost_guard_pricing_from_model_info(const json_t *model_info,
                                              double *in_out, double *out_out,
                                              char *source, size_t source_sz)
{
    if (source && source_sz) source[0] = '\0';
    if (in_out) *in_out = 0;
    if (out_out) *out_out = 0;
    if (!model_info || model_info->type != JSON_OBJECT) return 0;
    json_t *inp = json_obj_get((json_t *)model_info, "cost_input");
    json_t *out = json_obj_get((json_t *)model_info, "cost_output");
    int has = 0;
    if (inp && inp->type == JSON_NUMBER && inp->num_val > 0) has = 1;
    if (out && out->type == JSON_NUMBER && out->num_val > 0) has = 1;
    if (!has) return 0;
    if (in_out)  *in_out  = (inp && inp->type == JSON_NUMBER)  ? inp->num_val  : 0.0;
    if (out_out) *out_out = (out && out->type == JSON_NUMBER) ? out->num_val : 0.0;
    if (source && source_sz) snprintf(source, source_sz, "models.dev");
    return 1;
}

/* =========================================================================
 * expensive_model_warning (full port of the Python entry point) — added when
 * model_selection_guards.py was ported; _cost_guard calls this.
 * ========================================================================= */
#include "model_cost_guard.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define MCG_INPUT_COST_WARNING_THRESHOLD  20.0   /* Decimal("20") */
#define MCG_OUTPUT_COST_WARNING_THRESHOLD 100.0  /* Decimal("100") */
static const char MCG_GPT55_PRO_OPENROUTER_ID[] = "openai/gpt-5.5-pro";
static const char MCG_GPT55_SUGGESTION[] = "did you mean to select openai/gpt-5.5?";

/* agent.models_dev.PROVIDER_TO_MODELS_DEV (verbatim). */
static const struct { const char *provider; const char *models_dev; } MCG_PROVIDER_MAP[] = {
    {"openrouter", "openrouter"}, {"novita", "novita-ai"},
    {"anthropic", "anthropic"}, {"openai", "openai"},
    {"openai-codex", "openai"}, {"zai", "zai"},
    {"kimi", "kimi-for-coding"}, {"kimi-coding", "kimi-for-coding"},
    {"moonshot", "kimi-for-coding"}, {"stepfun", "stepfun"},
    {"kimi-coding-cn", "kimi-for-coding"}, {"minimax", "minimax"},
    {"minimax-oauth", "minimax"}, {"minimax-cn", "minimax-cn"},
    {"deepseek", "deepseek"}, {"alibaba", "alibaba"},
    {"qwen-oauth", "alibaba"}, {"copilot", "github-copilot"},
    {"ai-gateway", "vercel"}, {"opencode-zen", "opencode"},
    {"opencode-go", "opencode-go"}, {"kilocode", "kilo"},
    {"fireworks", "fireworks-ai"}, {"huggingface", "huggingface"},
    {"gemini", "google"}, {"google", "google"}, {"xai", "xai"},
    {"xai-oauth", "xai"}, {"xiaomi", "xiaomi"}, {"nvidia", "nvidia"},
    {"meta-ai", "meta"}, {"meta", "meta"}, {"groq", "groq"},
    {"mistral", "mistral"}, {"togetherai", "togetherai"},
    {"perplexity", "perplexity"}, {"cohere", "cohere"},
    {"ollama-cloud", "ollama-cloud"},
};

static char *mcg_strip_dup(const char *s)
{
    char *d = strdup(s ? s : "");
    if (!d) return NULL;
    size_t n = strlen(d), start = 0, end = n;
    while (start < end && (unsigned char)d[start] <= ' ') start++;
    while (end > start && (unsigned char)d[end - 1] <= ' ') end--;
    memmove(d, d + start, end - start);
    d[end - start] = '\0';
    return d;
}

static char *mcg_lower(const char *s)
{
    char *d = mcg_strip_dup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
    return d;
}

/* PoP: _known_models_dev_provider @ hermes_cli/model_cost_guard.py:_known_models_dev_provider */
const char *model_cost_guard_known_models_dev_provider(const char *provider)
{
    if (!provider) return NULL;
    char *norm = mcg_lower(provider);
    if (!norm || !norm[0]) { free(norm); return NULL; }
    const char *hit = NULL;
    for (size_t i = 0; i < sizeof(MCG_PROVIDER_MAP)/sizeof(MCG_PROVIDER_MAP[0]); i++) {
        if (strcmp(norm, MCG_PROVIDER_MAP[i].provider) == 0) {
            hit = MCG_PROVIDER_MAP[i].models_dev;
            break;
        }
    }
    free(norm);
    return hit;
}

/* PoP: _can_trust_model_info_pricing @ hermes_cli/model_cost_guard.py:_can_trust_model_info_pricing */
int model_cost_guard_can_trust_model_info_pricing(const char *provider,
                                                  const json_t *model_info)
{
    const char *expected = model_cost_guard_known_models_dev_provider(provider);
    if (!expected || !model_info || model_info->type != JSON_OBJECT)
        return 0;
    json_t *pid = json_obj_get((json_t *)model_info, "provider_id");
    const char *raw = (pid && pid->type == JSON_STRING) ? pid->str_val : "";
    char *actual = mcg_lower(raw);
    int ok = (!actual || !actual[0] || strcmp(actual, expected) == 0);
    free(actual);
    return ok;
}

void expensive_model_warning_free(ExpensiveModelWarning *w)
{
    if (!w) return;
    free(w->model); free(w->provider); free(w->source); free(w->message);
    free(w);
}

/* PoP: expensive_model_warning @ hermes_cli/model_cost_guard.py:expensive_model_warning */
ExpensiveModelWarning *expensive_model_warning(
    const char *model_name, const char *provider, const char *base_url,
    const char *api_key, const json_t *model_info,
    const model_cost_guard_seams *seams)
{
    char *model = mcg_strip_dup(model_name);
    if (!model) return NULL;
    if (!model[0]) { free(model); return NULL; }

    int has_input = 0, has_output = 0;
    double input_cost = 0.0, output_cost = 0.0;
    const char *source = "";

    if (model_cost_guard_can_trust_model_info_pricing(provider, model_info)) {
        char srcbuf[64];
        has_input = model_cost_guard_pricing_from_model_info(
            model_info, &input_cost, &output_cost, srcbuf, sizeof(srcbuf));
        if (has_input) {
            source = "models.dev";
            has_output = (output_cost > 0.0);
        }
    }
    if (!has_input && !has_output
        && model_cost_guard_known_models_dev_provider(provider)) {
        /* Python: get_model_info(provider, model) via models.dev; wrapped in
         * try/except pass. Network-coupled => seam callback or absent. */
        if (seams && seams->models_dev_lookup) {
            json_t *info = seams->models_dev_lookup(
                seams->models_dev_ctx,
                model_cost_guard_known_models_dev_provider(provider), model);
            char srcbuf[64];
            if (info && model_cost_guard_pricing_from_model_info(
                    info, &input_cost, &output_cost, srcbuf, sizeof(srcbuf))) {
                source = "models.dev";
                has_input = 1;
                has_output = (output_cost > 0.0);
            }
            if (info) json_free(info);
        }
    }
    if (!has_input && !has_output && seams && seams->pricing_lookup) {
        double in_c = 0.0, out_c = 0.0;
        const char *src2 = NULL;
        if (seams->pricing_lookup(seams->pricing_ctx, model, provider,
                                  base_url, api_key, &in_c, &out_c, &src2)) {
            input_cost = in_c;
            output_cost = out_c;
            has_input = 1;
            has_output = 1;
            source = src2 ? src2 : "";
        }
    }

    int is_gpt55_confusion =
        strcasecmp(model, MCG_GPT55_PRO_OPENROUTER_ID) == 0;
    int over_input = (has_input && input_cost > MCG_INPUT_COST_WARNING_THRESHOLD);
    int over_output = (has_output && output_cost > MCG_OUTPUT_COST_WARNING_THRESHOLD);
    if (!over_input && !over_output && !is_gpt55_confusion) {
        free(model);
        return NULL;
    }

    char in_money[32], out_money[32];
    if (has_input) snprintf(in_money, sizeof(in_money), "$%.2f/M", input_cost);
    else snprintf(in_money, sizeof(in_money), "unknown");
    if (has_output) snprintf(out_money, sizeof(out_money), "$%.2f/M", output_cost);
    else snprintf(out_money, sizeof(out_money), "unknown");

    size_t cap = strlen(model) + strlen(in_money) + strlen(out_money)
               + strlen(source ? source : "") + 512;
    char *msg = malloc(cap);
    if (!msg) { free(model); return NULL; }
    size_t off = 0;
    #define MCG_APPEND(...) do { \
        off += (size_t)snprintf(msg + off, cap - off, __VA_ARGS__); } while (0)
    MCG_APPEND("!!! EXPENSIVE MODEL WARNING !!!\n\n");
    MCG_APPEND("%s has known pricing above Hermes' safety threshold.\n", model);
    MCG_APPEND("Input tokens: %s\n", in_money);
    MCG_APPEND("Output tokens: %s\n", out_money);
    MCG_APPEND("Threshold: more than $20/M input tokens or more than $100/M output tokens.\n");
    if (source && source[0])
        MCG_APPEND("Pricing source: %s.\n", source);
    if (is_gpt55_confusion)
        MCG_APPEND("%s\n", MCG_GPT55_SUGGESTION);
    MCG_APPEND("Confirm only if you intend to use this model.");
    #undef MCG_APPEND

    ExpensiveModelWarning *w = calloc(1, sizeof(*w));
    if (!w) { free(msg); free(model); return NULL; }
    w->model = model;
    w->provider = mcg_strip_dup(provider);
    w->has_input_cost = has_input;
    w->input_cost_per_million = input_cost;
    w->has_output_cost = has_output;
    w->output_cost_per_million = output_cost;
    w->source = strdup((source && source[0]) ? source : "unknown");
    w->message = msg;
    return w;
}

