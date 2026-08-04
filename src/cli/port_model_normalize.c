/*
 * port_model_normalize.c — Faithful C port of hermes_cli/model_normalize.py.
 *
 * Centralises per-provider model-name normalisation so callers can write
 *     api_model = normalize_model_for_provider(user_input, provider)
 * The module is pure string logic (no IO); provider ids are expected to be
 * Hermes-canonical (already run through normalize_provider). We reuse
 * model_normalize_provider() from model_catalog.c for the alias resolution,
 * so provider aliases are not duplicated here.
 *
 * Functions ported (C name <- python name):
 *   _normalize_for_deepseek          <- _normalize_for_deepseek
 *   model_normalize_strip_vendor      <- _strip_vendor_prefix
 *   model_normalize_dots_to_hyphens   <- _dots_to_hyphens
 *   model_normalize_provider_alias    <- _normalize_provider_alias
 *   model_normalize_strip_match_prefix<- _strip_matching_provider_prefix
 *   model_normalize_detect_vendor     <- detect_vendor
 *   model_normalize_prepend_vendor    <- _prepend_vendor
 *   model_normalize_for_provider      <- normalize_model_for_provider
 *
 * PoP: hermes_cli/model_normalize.py
 */

/* strcasestr and friends are GNU extensions — musl (alpine) needs
 * _GNU_SOURCE to declare them; glibc exposes them by default. */
#define _GNU_SOURCE
#include "model_catalog.h"   /* model_normalize_provider() */
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

/* ── lookup tables (mirror _VENDOR_PREFIXES) ─────────────────────────── */
static const struct { const char *tok; const char *vendor; } VENDOR_PREFIXES[] = {
    {"claude", "anthropic"},
    {"gpt", "openai"},
    {"o1", "openai"},
    {"o3", "openai"},
    {"o4", "openai"},
    {"gemini", "google"},
    {"gemma", "google"},
    {"deepseek", "deepseek"},
    {"glm", "z-ai"},
    {"kimi", "moonshotai"},
    {"minimax", "minimax"},
    {"grok", "x-ai"},
    {"qwen", "qwen"},
    {"mimo", "xiaomi"},
    {"trinity", "arcee-ai"},
    {"nemotron", "nvidia"},
    {"llama", "meta-llama"},
    {"step", "stepfun"},
    {NULL, NULL},
};

/* Providers that consume vendor/model slugs. */
static bool is_aggregator(const char *p) {
    return !strcmp(p, "openrouter") || !strcmp(p, "nous") || !strcmp(p, "kilocode");
}
/* Strip matching provider/ prefix (zai, kimi-coding, minimax, ..., custom, gemini, xai). */
static bool is_matching_prefix_strip(const char *p) {
    static const char *set[] = {"zai","kimi-coding","kimi-coding-cn","minimax","minimax-oauth",
        "minimax-cn","alibaba","qwen-oauth","xiaomi","arcee","ollama-cloud","custom","gemini","xai",NULL};
    for (int i = 0; set[i]; i++) if (!strcmp(p, set[i])) return true;
    return false;
}
static bool is_lowercase_model_provider(const char *p) {
    return !strcmp(p, "xiaomi");
}
/* Keep dots (copilot / copilot-acp / openai-codex). */
static bool is_strip_vendor_only(const char *p) {
    return !strcmp(p, "copilot") || !strcmp(p, "copilot-acp") || !strcmp(p, "openai-codex");
}
/* Authoritative native (preserve as-is). */
static bool is_authoritative_native(const char *p) {
    return !strcmp(p, "huggingface");
}

/* ── DeepSeek canonical handling ─────────────────────────────────────── */

static bool deepseek_is_canonical(const char *b) {
    static const char *canon[] = {"deepseek-chat","deepseek-reasoner","deepseek-v4-pro","deepseek-v4-flash",NULL};
    for (int i = 0; canon[i]; i++) if (!strcmp(b, canon[i])) return true;
    return false;
}
/* ^deepseek-v<digit>([-.].+)?$  (V-series first-class IDs + dated variants). */
static bool deepseek_is_vseries(const char *b) {
    if (strncmp(b, "deepseek-v", 10) != 0) return false;
    if (!isdigit((unsigned char)b[10])) return false;
    /* remainder may be anything (".", "-", digits, letters) — accept. */
    return true;
}
static bool deepseek_has_reasoner_kw(const char *b) {
    static const char *kw[] = {"reasoner","r1","think","reasoning","cot",NULL};
    for (int i = 0; kw[i]; i++) if (strcasestr(b, kw[i])) return true;
    return false;
}

/* _normalize_for_deepseek — bare name (vendor prefix already stripped). */
/* PoP: model_normalize_for_deepseek @ hermes_cli/model_normalize.py:_normalize_for_deepseek */
const char *model_normalize_for_deepseek(const char *model_name)
{
    /* returns a static string; caller must not free */
    static char buf[128];
    if (!model_name || !model_name[0]) return "deepseek-chat";
    /* lower-case a working copy */
    char low[256];
    size_t n = strlen(model_name);
    if (n >= sizeof(low)) n = sizeof(low) - 1;
    for (size_t i = 0; i < n; i++) low[i] = (char)tolower((unsigned char)model_name[i]);
    low[n] = '\0';
    /* strip a vendor/ prefix if present (Python: bare = _strip_vendor_prefix(...).lower()) */
    const char *bare = low;
    char *slash = strchr(low, '/');
    if (slash) bare = slash + 1;

    if (deepseek_is_canonical(bare)) return bare;
    if (deepseek_is_vseries(bare)) return bare;
    if (deepseek_has_reasoner_kw(bare)) return "deepseek-reasoner";
    (void)buf;
    return "deepseek-chat";
}

/* ── helpers ─────────────────────────────────────────────────────────── */

/* _strip_vendor_prefix — drop a leading vendor/. Caller frees. */
char *model_normalize_strip_vendor(const char *model_name)
{
    if (!model_name) return strdup("");
    const char *slash = strchr(model_name, '/');
    if (slash) return strdup(slash + 1);
    return strdup(model_name);
}

/* _dots_to_hyphens — replace '.' with '-'. Caller frees. */
/* PoP: model_normalize_dots_to_hyphens @ hermes_cli/model_normalize.py:_dots_to_hyphens */
char *model_normalize_dots_to_hyphens(const char *model_name)
{
    if (!model_name) return strdup("");
    size_t n = strlen(model_name);
    char *out = malloc(n + 1);
    for (size_t i = 0; i <= n; i++) out[i] = (model_name[i] == '.') ? '-' : model_name[i];
    return out;
}

/* _normalize_provider_alias — resolve via model_normalize_provider(). Caller frees. */
/* PoP: model_normalize_provider_alias @ hermes_cli/model_normalize.py:_normalize_provider_alias */
char *model_normalize_provider_alias(const char *provider_name)
{
    if (!provider_name) return strdup("");
    char tmp[256];
    size_t n = strlen(provider_name);
    if (n >= sizeof(tmp)) n = sizeof(tmp) - 1;
    for (size_t i = 0; i < n; i++) tmp[i] = (char)tolower((unsigned char)provider_name[i]);
    tmp[n] = '\0';
    if (!tmp[0]) return strdup("");
    const char *norm = model_normalize_provider(tmp);
    return strdup(norm ? norm : tmp);
}

/* PoP: model_normalize_strip_match_prefix @ hermes_cli/model_normalize.py:_strip_matching_provider_prefix */
/* _strip_matching_provider_prefix — strip provider/ only when prefix == target. Caller frees. */
char *model_normalize_strip_match_prefix(const char *model_name, const char *target_provider)
{
    if (!model_name) return strdup("");
    const char *slash = strchr(model_name, '/');
    if (!slash) return strdup(model_name);
    /* prefix must be non-empty and remainder must be non-empty */
    if (slash == model_name) return strdup(model_name);
    if (slash[1] == '\0') return strdup(model_name);
    char *prefix = malloc((size_t)(slash - model_name) + 1);
    memcpy(prefix, model_name, (size_t)(slash - model_name));
    prefix[slash - model_name] = '\0';
    char *np = model_normalize_provider_alias(prefix);
    char *nt = model_normalize_provider_alias(target_provider);
    bool match = np && nt && strcmp(np, nt) == 0;
    free(np); free(nt);
    char *res = match ? strdup(slash + 1) : strdup(model_name);
    free(prefix);
    return res;
}

/* detect_vendor — first hyphen/version token -> vendor slug. Caller frees (or NULL). */
char *model_normalize_detect_vendor(const char *model_name)
{
    if (!model_name) return NULL;
    char tmp[256];
    size_t n = strlen(model_name);
    if (n >= sizeof(tmp)) n = sizeof(tmp) - 1;
    for (size_t i = 0; i < n; i++) tmp[i] = (char)tolower((unsigned char)model_name[i]);
    tmp[n] = '\0';
    /* already vendor/-prefixed? use the prefix directly. */
    char *slash = strchr(tmp, '/');
    if (slash) {
        if (slash == tmp) return NULL;
        char *p = malloc((size_t)(slash - tmp) + 1);
        memcpy(p, tmp, (size_t)(slash - tmp));
        p[slash - tmp] = '\0';
        return p;
    }
    /* first hyphen-delimited token, exact match */
    char *first = strdup(tmp);
    char *hyp = strchr(first, '-');
    if (hyp) *hyp = '\0';
    for (int i = 0; VENDOR_PREFIXES[i].tok; i++) {
        if (strcmp(first, VENDOR_PREFIXES[i].tok) == 0) { free(first); return strdup(VENDOR_PREFIXES[i].vendor); }
    }
    /* prefix starts-with match (handles qwen3.5 -> qwen, etc.) */
    for (int i = 0; VENDOR_PREFIXES[i].tok; i++) {
        if (strncmp(tmp, VENDOR_PREFIXES[i].tok, strlen(VENDOR_PREFIXES[i].tok)) == 0) {
            free(first); return strdup(VENDOR_PREFIXES[i].vendor);
        }
    }
    free(first);
    return NULL;
}

/* _prepend_vendor — prepend vendor/ when missing. Caller frees. */
/* PoP: model_normalize_prepend_vendor @ hermes_cli/model_normalize.py:_prepend_vendor */
char *model_normalize_prepend_vendor(const char *model_name)
{
    if (!model_name) return strdup("");
    if (strchr(model_name, '/')) return strdup(model_name);
    char *v = model_normalize_detect_vendor(model_name);
    if (!v) return strdup(model_name);
    size_t need = strlen(v) + 1 + strlen(model_name) + 1;
    char *out = malloc(need);
    snprintf(out, need, "%s/%s", v, model_name);
    free(v);
    return out;
}

/* PoP: normalize_copilot_model_id @ hermes_cli/models.py:normalize_copilot_model_id */
/* Copilot-specific resolver (pure part). Python's version also does a live
 * catalog lookup; that dynamic part is a REAL_GAP here — we resolve against
 * the static alias table and strip the vendor/prefix, reproducing the
 * documented doctest behavior. Returns a malloc'd string (caller frees). */
char *model_normalize_copilot_model_id(const char *model_id)
{
    if (!model_id) return strdup("");
    char raw[512];
    size_t n = strlen(model_id);
    if (n >= sizeof(raw)) n = sizeof(raw) - 1;
    memcpy(raw, model_id, n); raw[n] = '\0';

    /* static alias table (mirrors _COPILOT_MODEL_ALIASES) */
    static const struct { const char *k; const char *v; } ALIASES[] = {
        {"openai/gpt-5", "gpt-5-mini"}, {"openai/gpt-5-chat", "gpt-5-mini"},
        {"openai/gpt-5-mini", "gpt-5-mini"}, {"openai/gpt-5-nano", "gpt-5-mini"},
        {"openai/gpt-4.1", "gpt-4.1"}, {"openai/gpt-4.1-mini", "gpt-4.1"},
        {"openai/gpt-4.1-nano", "gpt-4.1"}, {"openai/gpt-4o", "gpt-4o"},
        {"openai/gpt-4o-mini", "gpt-4o-mini"}, {"openai/o1", "gpt-5.2"},
        {"openai/o1-mini", "gpt-5-mini"}, {"openai/o1-preview", "gpt-5.2"},
        {"openai/o3", "gpt-5.3-codex"}, {"openai/o3-mini", "gpt-5-mini"},
        {"openai/o4-mini", "gpt-5-mini"},
        {"anthropic/claude-opus-4.6", "claude-opus-4.6"},
        {"anthropic/claude-sonnet-4.6", "claude-sonnet-4.6"},
        {"anthropic/claude-sonnet-4", "claude-sonnet-4"},
        {"anthropic/claude-sonnet-4.5", "claude-sonnet-4.5"},
        {"anthropic/claude-haiku-4.5", "claude-haiku-4.5"},
        {"claude-opus-4-6", "claude-opus-4.6"}, {"claude-sonnet-4-6", "claude-sonnet-4.6"},
        {"claude-sonnet-4-0", "claude-sonnet-4"}, {"claude-sonnet-4-5", "claude-sonnet-4.5"},
        {"claude-haiku-4-5", "claude-haiku-4.5"},
        {"anthropic/claude-opus-4-6", "claude-opus-4.6"},
        {"anthropic/claude-sonnet-4-6", "claude-sonnet-4.6"},
        {"anthropic/claude-sonnet-4-0", "claude-sonnet-4"},
        {"anthropic/claude-sonnet-4-5", "claude-sonnet-4.5"},
        {"anthropic/claude-haiku-4-5", "claude-haiku-4.5"},
        {NULL, NULL},
    };

    /* direct alias */
    for (int i = 0; ALIASES[i].k; i++) if (strcmp(raw, ALIASES[i].k) == 0) return strdup(ALIASES[i].v);

    /* build candidate list: raw, raw-without-prefix, and -mini/-nano/-chat bases */
    char *cands[8]; int nc = 0;
    cands[nc++] = raw;
    char *slash = strchr(raw, '/');
    char remainder[512];
    if (slash) {
        size_t rl = strlen(slash + 1);
        if (rl >= sizeof(remainder)) rl = sizeof(remainder) - 1;
        memcpy(remainder, slash + 1, rl); remainder[rl] = '\0';
        cands[nc++] = remainder;
    }
    char *base = NULL;
    if (strlen(raw) > 5 && strcmp(raw + strlen(raw) - 5, "-mini") == 0) { base = strdup(raw); base[strlen(raw)-5] = '\0'; cands[nc++] = base; }
    if (strlen(raw) > 5 && strcmp(raw + strlen(raw) - 5, "-nano") == 0) { base = strdup(raw); base[strlen(raw)-5] = '\0'; cands[nc++] = base; }
    if (strlen(raw) > 5 && strcmp(raw + strlen(raw) - 5, "-chat") == 0) { base = strdup(raw); base[strlen(raw)-5] = '\0'; cands[nc++] = base; }

    char *result = NULL;
    for (int i = 0; i < nc; i++) {
        for (int j = 0; ALIASES[j].k; j++) {
            if (strcmp(cands[i], ALIASES[j].k) == 0) { result = strdup(ALIASES[j].v); break; }
        }
        if (result) break;
    }
    for (int i = 0; i < nc; i++) if (cands[i] != raw && cands[i] != remainder && cands[i] != base) free((void*)cands[i]);
    if (base && base != raw && base != remainder) free(base);

    if (result) return result;
    /* fallback: strip vendor/ prefix if present, else unchanged */
    if (slash) return strdup(remainder);
    return strdup(raw);
}

/* normalize_model_for_provider — primary entry point. Caller frees. */
char *model_normalize_for_provider(const char *model_input, const char *target_provider)
{
    if (!model_input) return strdup("");
    char *name = strdup(model_input);
    /* strip leading/trailing whitespace */
    char *s = name, *e = name + strlen(name);
    while (s < e && isspace((unsigned char)*s)) s++;
    while (e > s && isspace((unsigned char)e[-1])) e--;
    *e = '\0';
    char *trimmed = strdup(s);
    free(name); name = trimmed;
    if (!*name) { char *r = strdup(""); free(name); return r; }

    char *provider = model_normalize_provider_alias(target_provider);
    char *result = NULL;

    if (is_aggregator(provider)) {
        result = model_normalize_prepend_vendor(name);
    } else if (!strcmp(provider, "opencode-zen") || !strcmp(provider, "opencode-go")) {
        char *bare = name;
        char *sl = strchr(name, '/');
        if (sl) {
            char *after = sl + 1;
            if (*after) bare = after;
        }
        if (!strcmp(provider, "opencode-zen") && strncasecmp(bare, "claude-", 7) == 0) {
            result = model_normalize_dots_to_hyphens(bare);
        } else {
            result = strdup(bare);
        }
    } else if (!strcmp(provider, "anthropic")) {
        char *bare = model_normalize_strip_match_prefix(name, provider);
        if (strchr(bare, '/')) { result = bare; }
        else { result = model_normalize_dots_to_hyphens(bare); free(bare); }
    } else if (is_strip_vendor_only(provider)) {
        if (!strcmp(provider, "copilot") || !strcmp(provider, "copilot-acp")) {
            /* Python delegates to normalize_copilot_model_id (pure part here). */
            result = model_normalize_copilot_model_id(name);
        } else {
            char *stripped = model_normalize_strip_match_prefix(name, provider);
            if (strcmp(stripped, name) == 0 && strncmp(name, "openai/", 7) == 0) {
                char *after = name + 7;
                result = strdup(after);
                free(stripped);
            } else {
                result = stripped;
            }
        }
    } else if (!strcmp(provider, "deepseek")) {
        char *bare = model_normalize_strip_match_prefix(name, provider);
        if (strchr(bare, '/')) { result = bare; }
        else { result = strdup(model_normalize_for_deepseek(bare)); free(bare); }
    } else if (is_matching_prefix_strip(provider)) {
        result = model_normalize_strip_match_prefix(name, provider);
        if (is_lowercase_model_provider(provider)) {
            char *low = strdup(result);
            for (char *p = low; *p; p++) *p = (char)tolower((unsigned char)*p);
            free(result); result = low;
        }
    } else if (is_authoritative_native(provider)) {
        result = strdup(name);
    } else {
        result = strdup(name);
    }

    free(provider);
    free(name);
    return result;
}
