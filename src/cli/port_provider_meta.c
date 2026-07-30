/*
 * port_provider_meta.c — pure display-logic slice of hermes_cli/models.py:
 *   normalize_provider, provider_label, provider_group_for_slug, group_providers
 *
 * No network, no model catalog. Only static tables (below) drive resolution.
 *
 * PoP: normalize_provider         @ hermes_cli/models.py:normalize_provider
 * PoP: provider_label             @ hermes_cli/models.py:provider_label
 * PoP: provider_group_for_slug    @ hermes_cli/models.py:provider_group_for_slug
 * PoP: group_providers            @ hermes_cli/models.py:group_providers
 */

#include "provider_meta.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ── Static tables (auto-generated from hermes_cli/models.py) ────────── */

/* _PROVIDER_LABELS: slug -> label. */
static const struct { const char *slug; const char *label; } PROVIDER_LABELS[] = {
    { "alibaba", "Qwen Cloud" },
    { "alibaba-coding-plan", "Alibaba Cloud (Coding Plan)" },
    { "anthropic", "Anthropic" },
    { "arcee", "Arcee AI" },
    { "azure-foundry", "Azure Foundry" },
    { "bedrock", "AWS Bedrock" },
    { "copilot", "GitHub Copilot" },
    { "copilot-acp", "GitHub Copilot ACP" },
    { "custom", "Custom endpoint" },
    { "deepseek", "DeepSeek" },
    { "gemini", "Google AI Studio" },
    { "gmi", "GMI Cloud" },
    { "huggingface", "Hugging Face" },
    { "kilocode", "Kilo Code" },
    { "kimi-coding", "Kimi / Kimi Coding Plan" },
    { "kimi-coding-cn", "Kimi / Moonshot (China)" },
    { "lmstudio", "LM Studio" },
    { "minimax", "MiniMax" },
    { "minimax-cn", "MiniMax (China)" },
    { "minimax-oauth", "MiniMax (OAuth)" },
    { "moa", "Mixture of Agents" },
    { "nous", "Nous Portal" },
    { "novita", "NovitaAI" },
    { "nvidia", "NVIDIA NIM" },
    { "ollama-cloud", "Ollama Cloud" },
    { "openai-api", "OpenAI API" },
    { "openai-codex", "OpenAI Codex" },
    { "opencode-go", "OpenCode Go" },
    { "opencode-zen", "OpenCode Zen" },
    { "openrouter", "OpenRouter" },
    { "qwen-oauth", "Qwen OAuth (Portal)" },
    { "stepfun", "StepFun Step Plan" },
    { "tencent-tokenhub", "Tencent TokenHub" },
    { "vertex", "Google Vertex AI" },
    { "xai", "xAI" },
    { "xai-oauth", "xAI Grok OAuth (SuperGrok / Premium+)" },
    { "xiaomi", "Xiaomi MiMo" },
    { "zai", "Z.AI / GLM" },
    { NULL, NULL }
};

/* _PROVIDER_ALIASES: alias -> canonical slug. */
static const struct { const char *alias; const char *canon; } PROVIDER_ALIASES[] = {
    { "alibaba-cloud", "alibaba" },
    { "aliyun", "alibaba" },
    { "amazon", "bedrock" },
    { "amazon-bedrock", "bedrock" },
    { "arcee-ai", "arcee" },
    { "arceeai", "arcee" },
    { "aws", "bedrock" },
    { "aws-bedrock", "bedrock" },
    { "build-nvidia", "nvidia" },
    { "claude", "anthropic" },
    { "claude-code", "anthropic" },
    { "copilot-acp-agent", "copilot-acp" },
    { "dashscope", "alibaba" },
    { "deep-seek", "deepseek" },
    { "gcp-vertex", "vertex" },
    { "github", "copilot" },
    { "github-copilot", "copilot" },
    { "github-copilot-acp", "copilot-acp" },
    { "github-model", "copilot" },
    { "github-models", "copilot" },
    { "glm", "zai" },
    { "gmi-cloud", "gmi" },
    { "gmicloud", "gmi" },
    { "go", "opencode-go" },
    { "google", "gemini" },
    { "google-ai-studio", "gemini" },
    { "google-gemini", "gemini" },
    { "google-vertex", "vertex" },
    { "grok", "xai" },
    { "grok-oauth", "xai-oauth" },
    { "hf", "huggingface" },
    { "hugging-face", "huggingface" },
    { "huggingface-hub", "huggingface" },
    { "kilo", "kilocode" },
    { "kilo-code", "kilocode" },
    { "kilo-gateway", "kilocode" },
    { "kimi", "kimi-coding" },
    { "kimi-cn", "kimi-coding-cn" },
    { "lm-studio", "lmstudio" },
    { "lm_studio", "lmstudio" },
    { "lmstudio", "lmstudio" },
    { "mimo", "xiaomi" },
    { "minimax-china", "minimax-cn" },
    { "minimax-global", "minimax-oauth" },
    { "minimax-portal", "minimax-oauth" },
    { "minimax_cn", "minimax-cn" },
    { "minimax_oauth", "minimax-oauth" },
    { "moonshot", "kimi-coding" },
    { "moonshot-cn", "kimi-coding-cn" },
    { "nemotron", "nvidia" },
    { "nim", "nvidia" },
    { "novita-ai", "novita" },
    { "novitaai", "novita" },
    { "nvidia-nim", "nvidia" },
    { "ollama", "custom" },
    { "ollama_cloud", "ollama-cloud" },
    { "opencode", "opencode-zen" },
    { "opencode-go-sub", "opencode-go" },
    { "qwen", "alibaba" },
    { "qwen-portal", "qwen-oauth" },
    { "step", "stepfun" },
    { "stepfun-coding-plan", "stepfun" },
    { "tencent", "tencent-tokenhub" },
    { "tencent-cloud", "tencent-tokenhub" },
    { "tencentmaas", "tencent-tokenhub" },
    { "tokenhub", "tencent-tokenhub" },
    { "vertex-ai", "vertex" },
    { "vertexai", "vertex" },
    { "x-ai", "xai" },
    { "x-ai-oauth", "xai-oauth" },
    { "x.ai", "xai" },
    { "xai-grok-oauth", "xai-oauth" },
    { "xai-oauth", "xai-oauth" },
    { "xiaomi-mimo", "xiaomi" },
    { "z-ai", "zai" },
    { "z.ai", "zai" },
    { "zen", "opencode-zen" },
    { "zhipu", "zai" },
    { NULL, NULL }
};

/* PROVIDER_GROUPS: group_id -> (label, description, [members]). */
typedef struct {
    const char *gid;
    const char *label;
    const char *desc;
    const char *members[8];   /* NULL-terminated; max 8 members */
} provider_group_def_t;

static const provider_group_def_t PROVIDER_GROUPS[] = {
    { "kimi",     "Kimi / Moonshot", "Coding Plan, Moonshot global & China endpoints", { "kimi-coding", "kimi-coding-cn", NULL } },
    { "minimax",  "MiniMax",         "Global, OAuth Coding Plan & China endpoints",     { "minimax", "minimax-oauth", "minimax-cn", NULL } },
    { "xai",      "xAI Grok",        "Direct API or SuperGrok / Premium+ OAuth",        { "xai", "xai-oauth", NULL } },
    { "google",   "Google Gemini",   "Google AI Studio (API key)",                     { "gemini", NULL } },
    { "openai",   "OpenAI",          "Codex CLI or direct OpenAI API",                  { "openai-codex", "openai-api", NULL } },
    { "opencode", "OpenCode",        "Zen pay-as-you-go or Go subscription",            { "opencode-zen", "opencode-go", NULL } },
    { "copilot",  "GitHub Copilot",  "GitHub token API or copilot --acp process",       { "copilot", "copilot-acp", NULL } },
    { NULL, NULL, NULL, { NULL } }
};

/* ── Lookup helpers ─────────────────────────────────────────────────── */

static const char *label_for(const char *slug) {
    if (!slug) return NULL;
    for (size_t i = 0; PROVIDER_LABELS[i].slug; i++) {
        if (strcmp(PROVIDER_LABELS[i].slug, slug) == 0) return PROVIDER_LABELS[i].label;
    }
    return NULL;
}

static const char *alias_for(const char *alias) {
    if (!alias) return NULL;
    for (size_t i = 0; PROVIDER_ALIASES[i].alias; i++) {
        if (strcasecmp(PROVIDER_ALIASES[i].alias, alias) == 0) return PROVIDER_ALIASES[i].canon;
    }
    return NULL;
}

static const provider_group_def_t *group_for_member(const char *slug) {
    if (!slug) return NULL;
    for (size_t g = 0; PROVIDER_GROUPS[g].gid; g++) {
        for (size_t m = 0; PROVIDER_GROUPS[g].members[m]; m++) {
            if (strcasecmp(PROVIDER_GROUPS[g].members[m], slug) == 0) return &PROVIDER_GROUPS[g];
        }
    }
    return NULL;
}

/* ── Public API ────────────────────────────────────────────────────── */

const char *normalize_provider(const char *provider) {
    /* Python: (provider or "openrouter").strip().lower(); alias.get(norm, norm) */
    static char buf[128];
    const char *p = provider ? provider : "openrouter";
    /* "auto" passes through unchanged (lowercased) */
    if (strcasecmp(p, "auto") == 0) return "auto";
    /* lowercase + trim into buf */
    while (*p == ' ' || *p == '\t') p++;
    size_t j = 0;
    while (p[j] && p[j] != ' ' && p[j] != '\t' && j < sizeof(buf) - 1) {
        buf[j] = (char)(p[j] >= 'A' && p[j] <= 'Z' ? p[j] - 'A' + 'a' : p[j]);
        j++;
    }
    buf[j] = '\0';
    const char *canon = alias_for(buf);
    return canon ? canon : buf;
}

char *provider_label(const char *provider) {
    /* Python:
       original = (provider or "openrouter").strip()
       normalized = original.lower()
       if normalized == "auto": return "Auto"
       normalized = normalize_provider(normalized)
       return _PROVIDER_LABELS.get(normalized, original or "OpenRouter") */
    const char *orig = provider ? provider : "openrouter";
    /* lowercase + trim original into a temp */
    const char *p = orig;
    while (*p == ' ' || *p == '\t') p++;
    size_t n = strlen(p);
    while (n > 0 && (p[n-1] == ' ' || p[n-1] == '\t')) n--;
    char *low = malloc(n + 1);
    for (size_t i = 0; i < n; i++) low[i] = (char)(p[i] >= 'A' && p[i] <= 'Z' ? p[i] - 'A' + 'a' : p[i]);
    low[n] = '\0';

    char *result;
    if (strcmp(low, "auto") == 0) {
        result = strdup("Auto");
    } else {
        const char *canon = normalize_provider(low);   /* may return static buf */
        const char *lbl = label_for(canon);
        if (lbl) {
            result = strdup(lbl);
        } else {
            /* fall back to original (or "OpenRouter") */
            result = strdup(*orig ? orig : "OpenRouter");
        }
    }
    free(low);
    return result;
}

/* ── Public API ────────────────────────────────────────────────────── */

/* provider_group_for_slug() is defined in port_config_pure.c (canonical
 * single definition, PoP-annotated). This module reuses it via the shared
 * declaration in provider_meta.h. */

/* PoP: group_providers @ hermes_cli.models.py:group_providers */
provider_row_t *group_providers(const char *const *slugs) {
    if (!slugs) return NULL;

    /* Collect normalized present slugs in input order, dedupe. */
    size_t cap = 16, n = 0;
    char **norm = malloc(cap * sizeof(char *));
    for (size_t i = 0; slugs[i]; i++) {
        const char *s = slugs[i];
        if (!s) continue;
        /* trim + lowercase */
        while (*s == ' ' || *s == '\t') s++;
        size_t len = strlen(s);
        while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t')) len--;
        if (len == 0) continue;
        char *low = malloc(len + 1);
        for (size_t k = 0; k < len; k++) low[k] = (char)(s[k] >= 'A' && s[k] <= 'Z' ? s[k] - 'A' + 'a' : s[k]);
        low[len] = '\0';
        /* dedupe */
        bool dup = false;
        for (size_t k = 0; k < n; k++) if (strcmp(norm[k], low) == 0) { dup = true; break; }
        if (dup) { free(low); continue; }
        if (n >= cap) { cap *= 2; norm = realloc(norm, cap * sizeof(char *)); }
        norm[n++] = low;
    }

    /* Which present members each group has, in declaration order. */
    /* group_present[gid] = list of present members in declaration order. */
    const size_t NG = sizeof(PROVIDER_GROUPS) / sizeof(PROVIDER_GROUPS[0]) - 1;
    char **group_present[64];
    size_t group_n[64];
    for (size_t g = 0; g < NG; g++) { group_present[g] = NULL; group_n[g] = 0; }
    for (size_t g = 0; g < NG; g++) {
        for (size_t m = 0; PROVIDER_GROUPS[g].members[m]; m++) {
            const char *mem = PROVIDER_GROUPS[g].members[m];
            for (size_t k = 0; k < n; k++) {
                if (strcasecmp(norm[k], mem) == 0) {
                    group_present[g] = realloc(group_present[g], (group_n[g] + 1) * sizeof(char *));
                    group_present[g][group_n[g]++] = (char *)mem;  /* keep canonical member slug */
                    break;
                }
            }
        }
    }

    provider_row_t *head = NULL, *tail = NULL;
    int emitted[64];
    for (size_t g = 0; g < NG; g++) emitted[g] = 0;

    for (size_t k = 0; k < n; k++) {
        const char *s = norm[k];
        const provider_group_def_t *g = group_for_member(s);
        if (!g) {
            provider_row_t *row = calloc(1, sizeof(*row));
            row->kind = PROVIDER_ROW_SINGLE;
            row->slug = strdup(s);
            row->group_id = strdup("");
            row->label = strdup("");
            row->description = strdup("");
            row->members = NULL; row->n_members = 0;
            if (tail) tail->next = row; else head = row;
            tail = row;
            continue;
        }
        /* find group index */
        size_t gi = 0;
        while (gi < NG && &PROVIDER_GROUPS[gi] != g) gi++;
        if (emitted[gi]) continue;  /* already folded */
        emitted[gi] = 1;
        if (group_n[gi] <= 1) {
            provider_row_t *row = calloc(1, sizeof(*row));
            row->kind = PROVIDER_ROW_SINGLE;
            row->slug = strdup(group_present[gi][0]);
            row->group_id = strdup("");
            row->label = strdup("");
            row->description = strdup("");
            row->members = NULL; row->n_members = 0;
            if (tail) tail->next = row; else head = row;
            tail = row;
        } else {
            provider_row_t *row = calloc(1, sizeof(*row));
            row->kind = PROVIDER_ROW_GROUP;
            row->slug = strdup(group_present[gi][0]);
            row->group_id = strdup(g->gid);
            row->label = strdup(g->label);
            row->description = strdup(g->desc);
            row->n_members = group_n[gi];
            row->members = calloc(group_n[gi] + 1, sizeof(char *));
            for (size_t m = 0; m < group_n[gi]; m++) row->members[m] = strdup(group_present[gi][m]);
            row->members[group_n[gi]] = NULL;
            if (tail) tail->next = row; else head = row;
            tail = row;
        }
    }

    /* free temporaries */
    for (size_t k = 0; k < n; k++) free(norm[k]);
    free(norm);
    for (size_t g = 0; g < NG; g++) free(group_present[g]);
    return head;
}

void provider_group_rows_free(provider_row_t *rows) {
    provider_row_t *r = rows;
    while (r) {
        provider_row_t *nx = r->next;
        free(r->slug);
        free(r->group_id);
        free(r->label);
        free(r->description);
        if (r->members) {
            for (size_t i = 0; r->members[i]; i++) free(r->members[i]);
            free(r->members);
        }
        free(r);
        r = nx;
    }
}
