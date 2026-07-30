/* port_billing_links.c — faithful C11 port of agent/billing_links.py
 *
 * Provider-agnostic billing/credit-recovery link resolution. Pure logic;
 * no IO, no network. The Python module exposes:
 *   - BillingBlock dataclass (provider, provider_label, model, billing_url,
 *     is_nous, message) + to_dict()
 *   - is_nous_inference_route(provider, base_url)
 *   - _nous_billing_url()
 *   - _resolve_provider_link(slug, base_url)
 *   - build_billing_block(provider, base_url, model, message)
 *
 * Reuse (no reimplementation):
 *   - url_host_matches()            (include/hermes_url_safety.h)
 *   - is_nous_inference_route()     (src/agent/conversation_loop.c)
 *   - libjson (json_t, json_object, json_set, json_dumps)
 *
 * Opaque struct pattern: the definition of `struct billing_block` lives only
 * here; consumers use the accessors in billing_links.h.
 */

#include "billing_links.h"
#include <stddef.h>              /* size_t, needed before project headers */
#include "hermes_url_safety.h"   /* url_host_matches */
#include "hermes_json.h"         /* libjson: json_t, json_object, json_set, json_dumps */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* Reuse the already-ported is_nous_inference_route() (conversation_loop.c). */
extern bool is_nous_inference_route(const char *provider, const char *base_url);

/* Single source of truth: internal slug(s) + base_url host(s) -> billing page.
 * Curated "add credits / manage billing" landing pages. Hosts back the
 * OpenAI-compatible fallback where the slug is a generic bucket but base_url
 * reveals the real upstream. An unknown provider degrades to a readable label
 * with no invented URL. Order matters only for the host-scan fallback. */
typedef struct {
    const char *label;
    const char *url;
    const char *const *slugs;
    int        n_slugs;
    const char *const *hosts;
    int        n_hosts;
} _provider_t;

/* Slug tables (NULL-terminated arrays of string literals). */
static const char *k_openai_slugs[]  = {"openai", NULL};
static const char *k_openai_hosts[]  = {"api.openai.com", NULL};
static const char *k_anthropic_slugs[] = {"anthropic", NULL};
static const char *k_anthropic_hosts[] = {"api.anthropic.com", NULL};
static const char *k_openrouter_slugs[] = {"openrouter", NULL};
static const char *k_openrouter_hosts[] = {"openrouter.ai", NULL};
static const char *k_xai_slugs[] = {"xai", "xai-oauth", NULL};
static const char *k_xai_hosts[] = {"api.x.ai", NULL};
static const char *k_deepseek_slugs[] = {"deepseek", NULL};
static const char *k_deepseek_hosts[] = {"api.deepseek.com", NULL};
static const char *k_groq_slugs[] = {"groq", NULL};
static const char *k_groq_hosts[] = {"api.groq.com", NULL};
static const char *k_mistral_slugs[] = {"mistral", NULL};
static const char *k_mistral_hosts[] = {"api.mistral.ai", NULL};
static const char *k_together_slugs[] = {"together", NULL};
static const char *k_together_hosts[] = {"api.together.ai", "api.together.xyz", NULL};
static const char *k_fireworks_slugs[] = {"fireworks", NULL};
static const char *k_fireworks_hosts[] = {"fireworks.ai", NULL};
static const char *k_perplexity_slugs[] = {"perplexity", NULL};
static const char *k_perplexity_hosts[] = {"perplexity.ai", NULL};
static const char *k_google_slugs[] = {"google", "gemini", NULL};
static const char *k_google_hosts[] = {"generativelanguage.googleapis.com", NULL};
static const char *k_cohere_slugs[] = {"cohere", NULL};
static const char *k_cohere_hosts[] = {NULL};   /* no host fallback */
static const char *k_moonshot_slugs[] = {"moonshot", NULL};
static const char *k_moonshot_hosts[] = {NULL};
static const char *k_nvidia_slugs[] = {"nvidia", NULL};
static const char *k_nvidia_hosts[] = {NULL};

static const _provider_t _PROVIDERS[] = {
    {"OpenAI",        "https://platform.openai.com/settings/organization/billing",        k_openai_slugs,    2, k_openai_hosts,    1},
    {"Anthropic",     "https://console.anthropic.com/settings/billing",                  k_anthropic_slugs, 2, k_anthropic_hosts, 1},
    {"OpenRouter",    "https://openrouter.ai/settings/credits",                          k_openrouter_slugs, 2, k_openrouter_hosts, 1},
    {"xAI",           "https://console.x.ai/team/default/billing",                       k_xai_slugs,        3, k_xai_hosts,        1},
    {"DeepSeek",      "https://platform.deepseek.com/top_up",                            k_deepseek_slugs,   2, k_deepseek_hosts,  1},
    {"Groq",          "https://console.groq.com/settings/billing",                      k_groq_slugs,       2, k_groq_hosts,       1},
    {"Mistral",       "https://console.mistral.ai/billing",                             k_mistral_slugs,    2, k_mistral_hosts,   1},
    {"Together AI",   "https://api.together.ai/settings/billing",                       k_together_slugs,   2, k_together_hosts,  2},
    {"Fireworks AI",  "https://fireworks.ai/account/billing",                           k_fireworks_slugs,  2, k_fireworks_hosts, 1},
    {"Perplexity",    "https://www.perplexity.ai/settings/api",                         k_perplexity_slugs, 2, k_perplexity_hosts, 1},
    {"Google AI",     "https://aistudio.google.com/app/billing",                        k_google_slugs,     3, k_google_hosts,    1},
    {"Cohere",        "https://dashboard.cohere.com/billing",                           k_cohere_slugs,     2, k_cohere_hosts,    0},
    {"Moonshot AI",   "https://platform.moonshot.ai/console/pay",                       k_moonshot_slugs,   2, k_moonshot_hosts,  0},
    {"NVIDIA",        "https://build.nvidia.com/settings/billing",                      k_nvidia_slugs,     2, k_nvidia_hosts,    0},
};
static const int _N_PROVIDERS = (int)(sizeof(_PROVIDERS) / sizeof(_PROVIDERS[0]));

/* Opaque struct definition (hidden from consumers). */
struct billing_block {
    char *provider;
    char *provider_label;
    char *model;
    char *billing_url;   /* may be NULL -> stored as "" */
    bool  is_nous;
    char *message;
};

/* Faithful port of hermes-agent/agent/billing_links.py */

/* PoP: billing_links__nous_billing_url @ agent/billing_links.py:_nous_billing_url */
/* Best-effort Nous portal billing URL (text-surface fallback; Nous prefers
 * the in-app flow). Nous_portal_billing_url is not yet ported, so we fall back
 * to the canonical portal URL exactly as the Python try/except does. */
static const char *_nous_billing_url(void) {
    /* In Python this delegates to hermes_cli.nous_account.nous_portal_billing_url(None)
     * and falls back to the literal on any exception. The C port reuses the same
     * fallback literal; the in-app flow is preferred by surfaces anyway. */
    return "https://portal.nousresearch.com/billing";
}

/* PoP: billing_links__resolve_provider_link @ agent/billing_links.py:_resolve_provider_link */
/* Resolve (label, url): exact slug -> base_url host -> readable-label fallback.
 * Caller must free *out_label and *out_url (strdup'd). url may be NULL. */
static void _resolve_provider_link(const char *slug, const char *base_url,
                                   char **out_label, char **out_url) {
    if (out_label) *out_label = NULL;
    if (out_url)   *out_url   = NULL;

    if (slug && *slug) {
        for (int i = 0; i < _N_PROVIDERS; i++) {
            const _provider_t *p = &_PROVIDERS[i];
            for (int s = 0; s < p->n_slugs; s++) {
                if (p->slugs[s] && strcmp(p->slugs[s], slug) == 0) {
                    if (out_label) *out_label = strdup(p->label);
                    if (out_url)   *out_url   = strdup(p->url);
                    return;
                }
            }
        }
    }

    if (base_url && *base_url) {
        for (int i = 0; i < _N_PROVIDERS; i++) {
            const _provider_t *p = &_PROVIDERS[i];
            for (int h = 0; h < p->n_hosts; h++) {
                if (p->hosts[h] && url_host_matches(base_url, p->hosts[h])) {
                    if (out_label) *out_label = strdup(p->label);
                    if (out_url)   *out_url   = strdup(p->url);
                    return;
                }
            }
        }
    }

    /* Degrade to a readable label with no invented URL. Mirror Python's
     * slug.replace("_"," ").replace("-"," ").strip().title(): title-case
     * each word (first alpha upper, rest lower), spaces where _/- were. */
    char buf[256];
    int pos = 0;
    bool cap_next = true;
    for (const char *c = slug ? slug : ""; *c && pos < (int)sizeof(buf) - 1; c++) {
        if (*c == '_' || *c == '-') { buf[pos++] = ' '; cap_next = true; }
        else if (cap_next) { buf[pos++] = (char)toupper((unsigned char)*c); cap_next = false; }
        else { buf[pos++] = (char)tolower((unsigned char)*c); }
    }
    buf[pos] = '\0';
    char *label = strdup(buf[0] ? buf : "your provider");
    if (out_label) *out_label = label;
    if (out_url)   *out_url   = NULL;
}

/* PoP: billing_links_build_billing_block @ agent/billing_links.py:build_billing_block */
billing_block_t *billing_block_build(const char *provider,
                                     const char *base_url,
                                     const char *model,
                                     const char *message) {
    billing_block_t *b = calloc(1, sizeof(*b));
    if (!b) return NULL;

    const char *prov = provider ? provider : "";
    const char *mdl  = model ? model : "";
    const char *msg  = message ? message : "";

    /* slug = strip/lower of provider (mirrors Python (provider or '').strip().lower()) */
    char slug[256];
    {
        int o = 0;
        for (const char *c = prov; *c && o < (int)sizeof(slug) - 1; c++)
            if (*c != ' ') slug[o++] = (char)tolower((unsigned char)*c);
        slug[o] = '\0';
    }

    b->provider = strdup(slug[0] ? slug : "nous");
    b->model    = strdup(mdl);
    b->message  = strdup(msg);

    if (is_nous_inference_route(prov, base_url)) {
        b->provider_label = strdup("Nous Portal");
        b->billing_url    = strdup(_nous_billing_url());
        b->is_nous        = true;
    } else {
        char *lbl = NULL, *url = NULL;
        _resolve_provider_link(slug, base_url ? base_url : "", &lbl, &url);
        b->provider_label = lbl ? lbl : strdup("your provider");
        b->billing_url    = url;   /* may be NULL -> JSON null (matches Python None) */
        b->is_nous        = false;
    }
    return b;
}

void billing_block_free(billing_block_t *b) {
    if (!b) return;
    free(b->provider);
    free(b->provider_label);
    free(b->model);
    free(b->billing_url);
    free(b->message);
    free(b);
}

const char *billing_block_provider(const billing_block_t *b)      { return b ? b->provider : ""; }
const char *billing_block_provider_label(const billing_block_t *b){ return b ? b->provider_label : ""; }
const char *billing_block_model(const billing_block_t *b)         { return b ? b->model : ""; }
const char *billing_block_billing_url(const billing_block_t *b)   { return b ? (b->billing_url ? b->billing_url : "") : ""; }
bool         billing_block_is_nous(const billing_block_t *b)      { return b ? b->is_nous : false; }
const char *billing_block_message(const billing_block_t *b)       { return b ? b->message : ""; }

bool billing_links_is_nous_inference_route(const char *provider,
                                            const char *base_url) {
    return is_nous_inference_route(provider, base_url);
}

/* PoP: billing_links_build_billing_block @ agent/billing_links.py:BillingBlock.to_dict */
/* Faithful to_dict(): returns the asdict() JSON via libjson. Caller frees. */
char *billing_block_to_json(const billing_block_t *b) {
    if (!b) return strdup("null");
    json_t *o = json_object();
    if (!o) return strdup("null");
    json_set(o, "provider",       json_string(b->provider ? b->provider : ""));
    json_set(o, "provider_label", json_string(b->provider_label ? b->provider_label : ""));
    json_set(o, "model",          json_string(b->model ? b->model : ""));
    json_set(o, "billing_url", b->billing_url ? json_string(b->billing_url) : json_null());
    json_set(o, "is_nous",        json_bool(b->is_nous));
    json_set(o, "message",        json_string(b->message ? b->message : ""));
    char *s = json_dumps(o, 0);
    json_free(o);
    return s ? s : strdup("{}");
}
