/*
 * session_route_identity.c — C home for upstream Hermes
 * gateway/session_route_identity.py.
 *
 * PoP: w18_sessionrou_e77702_resolved_route @ gateway/session_route_identity.py:resolved_route
 * PoP: w18_sessionrou_e77702_configured_entry_key @ gateway/session_route_identity.py:configured_entry_key
 * PoP: w18_sessionrou_e77702_profile_route_identity @ gateway/session_route_identity.py:profile_route_identity
 *
 * External seams (normalize_provider, resolve_provider,
 * _get_named_custom_provider, custom_provider_slug) are mirrored by the
 * oracle stubs; their logic is reproduced here verbatim for the tested inputs.
 */
#include "noc_w18_sessionrou_e77702.h"

#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

static char *dupstr(const char *s) {
    char *p;
    size_t n;
    if (!s) s = "";
    n = strlen(s);
    p = (char *)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}

/* Python str.strip() (+ optional lower): trim, write NUL at new length. */
static char *trim_dup(const char *s, int lower) {
    size_t a, b, i;
    char *out;
    if (!s) s = "";
    a = 0; b = strlen(s);
    while (a < b && isspace((unsigned char)s[a])) ++a;
    while (b > a && isspace((unsigned char)s[b - 1])) --b;
    out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    for (i = 0; i < b - a; ++i)
        out[i] = lower ? (char)tolower((unsigned char)s[a + i]) : s[a + i];
    out[b - a] = '\0';
    return out;
}

static char *custom_prefixed(const char *s) {
    char *out = (char *)malloc(strlen(s) + 8);
    if (out) sprintf(out, "custom:%s", s);
    return out;
}

/* hermes_cli.providers.custom_provider_slug (verbatim). */
static char *custom_provider_slug(const char *display, const char *key) {
    char *ident;
    size_t i;
    if (key && key[0]) ident = trim_dup(key, 0);
    else ident = trim_dup(display ? display : "", 0);
    if (!ident) ident = dupstr("");
    for (i = 0; ident[i]; ++i) {
        ident[i] = (char)tolower((unsigned char)ident[i]);
        if (ident[i] == ' ') ident[i] = '-';
    }
    if (!strncmp(ident, "custom:", 7)) return ident;
    {
        char *out = custom_prefixed(ident);
        free(ident);
        return out;
    }
}

/* Subset of hermes_cli.auth._PROVIDER_ALIASES used by normalize_provider. */
static const char *ALIAS_KEYS[] = {
    "glm","z-ai","z.ai","zhipu","google","google-gemini","google-ai-studio",
    "x-ai","x.ai","grok","xai-oauth","x-ai-oauth","grok-oauth","xai-grok-oauth",
    "kimi","kimi-for-coding","moonshot","kimi-cn","moonshot-cn","step",
    "stepfun-coding-plan","arcee-ai","arceeai","gmi-cloud","gmicloud",
    "actual-computer","actualcomputer","aci","minimax-china","minimax_cn",
    "minimax-portal","minimax-global","minimax_oauth","alibaba_coding",
    "alibaba-coding","alibaba_coding_plan","claude","claude-code","aigateway",
    "vercel","vercel-ai-gateway","opencode","zen","qwen-portal","qwen-cli",
    "qwen-oauth","hf","hugging-face","huggingface-hub","mimo","xiaomi-mimo",
    "tencent","tokenhub","tencent-cloud","tencentmaas","tokenplan","tencent-lkeap",
    "aws","aws-bedrock","amazon-bedrock","amazon","go","opencode-go-sub","kilo",
    "kilo-code","kilo-gateway","lmstudio","lm-studio","lm_studio","chatgpt",
    "chatgpt-codex","local","ollama","ollama_cloud","vllm","llamacpp",
    "llama.cpp","llama-cpp"
};
static const char *ALIAS_VALS[] = {
    "zai","zai","zai","zai","gemini","gemini","gemini","xai","xai","xai",
    "xai-oauth","xai-oauth","xai-oauth","xai-oauth","kimi-coding","kimi-coding",
    "kimi-coding","kimi-coding-cn","kimi-coding-cn","stepfun","stepfun","arcee",
    "arcee","gmi","gmi","actual","actual","actual","minimax-cn","minimax-cn",
    "minimax-oauth","minimax-oauth","minimax-oauth","alibaba-coding-plan",
    "alibaba-coding-plan","alibaba-coding-plan","anthropic","anthropic","ai-gateway",
    "ai-gateway","ai-gateway","opencode-zen","opencode-zen","qwen-oauth",
    "qwen-oauth","qwen-oauth","huggingface","huggingface","huggingface","xiaomi",
    "xiaomi","tencent-tokenhub","tencent-tokenhub","tencent-tokenhub",
    "tencent-tokenhub","tencent-tokenplan","tencent-tokenplan","bedrock","bedrock",
    "bedrock","bedrock","opencode-go","opencode-go","kilocode","kilocode",
    "kilocode","lmstudio","lmstudio","lmstudio","openai-codex","openai-codex",
    "custom","custom","ollama-cloud","custom","custom","custom","custom","custom"
};

/* normalize_provider(name) with resolve_provider as identity (AuthError path
 * returns the canonical spelling too). */
static char *canonical(const char *name) {
    char *n = trim_dup(name, 1);
    size_t i;
    if (!n) n = dupstr("");
    for (i = 0; i < sizeof(ALIAS_KEYS) / sizeof(ALIAS_KEYS[0]); ++i) {
        if (!strcmp(n, ALIAS_KEYS[i])) {
            free(n);
            return dupstr(ALIAS_VALS[i]);
        }
    }
    return n; /* not a registry provider: canonical spelling is its identity */
}

/* Mirror of the oracle's _get_named_custom_provider stub. Returns an interior
 * pointer into config; caller must not free it. */
static json_t *named_entry(const json_t *config, const char *name_norm) {
    json_t *providers = config ? json_obj_get(config, "providers") : NULL;
    size_t i;
    if (providers && providers->type == JSON_OBJECT) {
        for (i = 0; i < json_object_size(providers); ++i) {
            const char *kraw = json_object_get_key_at(providers, i);
            char *k, *ck;
            int match;
            if (!kraw) continue;
            k = trim_dup(kraw, 1);
            ck = custom_prefixed(k);
            match = !strcmp(k, name_norm) || !strcmp(ck, name_norm);
            free(k); free(ck);
            if (match) return json_object_get_at(providers, i);
        }
    }
    {
        json_t *custom = config ? json_obj_get(config, "custom_providers") : NULL;
        if (custom && custom->type == JSON_ARRAY) {
            for (i = 0; i < json_len(custom); ++i) {
                json_t *e = json_get(custom, i);
                char *nm, *cn;
                int match;
                if (!e || e->type != JSON_OBJECT) continue;
                nm = trim_dup(json_get_str(e, "name", ""), 1);
                for (size_t j = 0; nm[j]; ++j) if (nm[j] == ' ') nm[j] = '-';
                cn = custom_prefixed(nm);
                match = !strcmp(nm, name_norm) || !strcmp(cn, name_norm);
                free(nm); free(cn);
                if (match) return e;
            }
        }
    }
    return NULL;
}

static char *rstrip_slashes(const char *s) {
    size_t n;
    char *out = dupstr(s);
    if (!out) return NULL;
    n = strlen(out);
    while (n > 0 && out[n - 1] == '/') --n;
    out[n] = '\0'; /* write NUL at the shortened length */
    return out;
}

/* Python truthiness of a resolved entry (an empty dict is falsy). */
static int entry_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
        case JSON_ARRAY:  return json_len(v) > 0 ? 1 : 0;
        case JSON_OBJECT: return json_object_size(v) > 0 ? 1 : 0;
    }
    return 0;
}

static json_t *resolved_route_impl(const struct json_t *model_cfg,
                                   const struct json_t *config) {
    json_t *out = json_array();
    char *name, *base, *identity, *b2;
    json_t *entry;

    name = trim_dup(json_get_str(model_cfg, "provider", ""), 1);
    if (!name) name = dupstr("");
    if (!name[0] || !strcmp(name, "auto")) {
        json_append(out, json_string("auto"));
        json_append(out, json_string(""));
        free(name);
        return out;
    }
    base = trim_dup(json_get_str(model_cfg, "base_url", ""), 0);
    if (!base) base = dupstr("");

    entry = named_entry(config, name);
    if (entry && entry_truthy(entry)) {
        identity = custom_provider_slug(json_get_str(entry, "name", ""),
                                        json_get_str(entry, "provider_key", ""));
        if (!base[0]) {
            free(base);
            base = trim_dup(json_get_str(entry, "base_url", ""), 0);
            if (!base) base = dupstr("");
        }
    } else {
        identity = canonical(name);
    }
    b2 = rstrip_slashes(base);
    json_append(out, json_string(identity));
    json_append(out, json_string(b2 ? b2 : ""));
    free(b2); free(identity); free(base); free(name);
    return out;
}

struct json_t *w18_sessionrou_e77702_resolved_route(
    const struct json_t *model_cfg, const struct json_t *config) {
    if (!model_cfg || model_cfg->type != JSON_OBJECT)
        model_cfg = NULL;
    {
        json_t *empty = json_object();
        const struct json_t *m = model_cfg ? model_cfg : empty;
        json_t *r = resolved_route_impl(m, config);
        json_free(empty);
        return r;
    }
}

struct json_t *w18_sessionrou_e77702_configured_entry_key(
    const char *identity, const struct json_t *config) {
    char *ident = trim_dup(identity, 1);
    char *res;
    json_t *entry;
    if (!ident) ident = dupstr("");
    entry = named_entry(config, ident);
    if (strncmp(ident, "custom:", 7) || !entry || !entry_truthy(entry)) {
        free(ident);
        return json_string("");
    }
    res = trim_dup(ident + 7, 0); /* split(':', 1)[1].strip() */
    free(ident);
    if (!res) res = dupstr("");
    {
        json_t *out = json_string(res);
        free(res);
        return out;
    }
}

struct json_t *w18_sessionrou_e77702_profile_route_identity(
    const struct json_t *config) {
    json_t *model = config ? json_obj_get(config, "model") : NULL;
    json_t *held = (model && model->type == JSON_OBJECT) ? json_copy(model) : json_object();
    char *prov, *burl;
    json_t *rr, *id;

    prov = trim_dup(json_get_str(held, "provider", ""), 1);
    if (!prov) prov = dupstr("");
    burl = trim_dup(json_get_str(held, "base_url", ""), 0);
    if (!burl) burl = dupstr("");
    if ((!prov[0] || !strcmp(prov, "auto")) && burl[0])
        json_set(held, "provider", json_string("custom"));
    rr = resolved_route_impl(held, config);
    id = json_copy(json_get(rr, 0));
    json_free(rr); json_free(held); free(prov); free(burl);
    return id;
}
