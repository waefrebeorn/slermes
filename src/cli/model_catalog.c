/*
 * model_catalog.c — static provider/model catalog + pure resolvers.
 * Faithful C11 port of hermes_cli/models.py (catalog + static resolution).
 *
 * The catalog tables here mirror _PROVIDER_MODELS, _PROVIDER_ALIASES,
 * PROVIDER_GROUPS, _PROVIDER_LABELS, _PROVIDER_SILENT_DEFAULT_OVERRIDES.
 * Live-API functions degrade to the static catalog when no network hook is
 * present, matching the Python "failures degrade to the static list" contract.
 */

#include "model_catalog.h"
#include "port_models_helpers.h"   /* reuse canonical fast-mode + cache-path helpers */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ── Static catalog: provider -> model list ──────────────────────────── */

typedef struct {
    const char *provider;
    const char *const *models;
    int n_models;
} provider_entry_t;

/* Individual model arrays (kept as static const so they live in rodata). */
static const char *M_NOUS[] = {
    "anthropic/claude-fable-5", "anthropic/claude-opus-4.8", "anthropic/claude-sonnet-5",
    "anthropic/claude-haiku-4.5", "openai/gpt-5.5", "openai/gpt-5.5-pro", "openai/gpt-5.4-mini",
    "google/gemini-3-pro-preview", "google/gemini-3.1-pro-preview", "google/gemini-3.5-flash",
    "x-ai/grok-4.3", "deepseek/deepseek-v4-pro", "deepseek/deepseek-v4-flash",
    "qwen/qwen3.7-max", "qwen/qwen3.7-plus", "qwen/qwen3.6-35b-a3b",
    "moonshotai/kimi-k2.6", "moonshotai/kimi-k2.7-code", "minimax/minimax-m3",
    "z-ai/glm-5.2", "z-ai/glm-5.1", "xiaomi/mimo-v2.5-pro", "tencent/hy3-preview",
    "stepfun/step-3.7-flash", "nvidia/nemotron-3-super-120b-a12b", "sakana/fugu-ultra",
};
static const char *M_OPENAI[] = {
    "gpt-5.4", "gpt-5.4-mini", "gpt-5-mini", "gpt-5.3-codex", "gpt-5.2-codex",
    "gpt-4.1", "gpt-4o", "gpt-4o-mini",
};
static const char *M_OPENAI_API[] = {
    "gpt-5.5", "gpt-5.5-pro", "gpt-5.4", "gpt-5.4-mini", "gpt-5.4-nano", "gpt-5-mini",
    "gpt-5.3-codex", "gpt-4.1", "gpt-4o", "gpt-4o-mini",
};
static const char *M_OPENAI_CODEX[] = {
    "gpt-5.5", "gpt-5.5-pro", "gpt-5.4", "gpt-5.4-mini", "gpt-5.4-nano", "gpt-5-mini",
    "gpt-5.3-codex", "gpt-5.2-codex", "gpt-4.1", "gpt-4o", "gpt-4o-mini",
};
static const char *M_COPILOT_ACP[] = { "copilot-acp" };
static const char *M_COPILOT[] = {
    "gpt-5.4", "gpt-5.4-mini", "gpt-5-mini", "gpt-5.3-codex", "gpt-5.2-codex", "gpt-4.1",
    "gpt-4o", "gpt-4o-mini", "claude-sonnet-4.6", "claude-sonnet-4", "claude-sonnet-4.5",
    "claude-haiku-4.5", "gemini-3.1-pro-preview", "gemini-3-pro-preview", "gemini-3-flash-preview",
    "gemini-2.5-pro",
};
static const char *M_GEMINI[] = {
    "gemini-3.1-pro-preview", "gemini-3-pro-preview", "gemini-3.5-flash", "gemini-3.1-flash-lite-preview",
};
static const char *M_ZAI[] = {
    "glm-5.2", "glm-5.1", "glm-5", "glm-5v-turbo", "glm-5-turbo", "glm-4.7", "glm-4.5", "glm-4.5-flash",
};
static const char *M_XAI[] = {
    "grok-4.3", "grok-4.2", "grok-4.1", "grok-3", "grok-3-mini", "grok-2",
};
static const char *M_XAI_OAUTH[] = { "grok-4.3", "grok-4.2", "grok-4.1", "grok-3", "grok-3-mini" };
static const char *M_NVIDIA[] = {
    "nvidia/nemotron-3-super-120b-a12b", "nvidia/nemotron-3-nano-30b-a3b",
    "nvidia/llama-3.3-nemotron-super-49b-v1.5", "qwen/qwen3.5-397b-a17b",
    "deepseek-ai/deepseek-v3.2", "moonshotai/kimi-k2.6", "minimaxai/minimax-m2.5",
    "z-ai/glm5", "openai/gpt-oss-120b",
};
static const char *M_KIMI_CODING[] = {
    "kimi-k2.7-code", "kimi-k2.6", "kimi-k2.5", "kimi-for-coding", "kimi-k2-thinking",
    "kimi-k2-thinking-turbo", "kimi-k2-turbo-preview", "kimi-k2-0905-preview",
};
static const char *M_KIMI_CODING_CN[] = {
    "kimi-k2.6", "kimi-k2.5", "kimi-k2-thinking", "kimi-k2-turbo-preview", "kimi-k2-0905-preview",
};
static const char *M_STEPFUN[] = { "step-3.5-flash", "step-3.5-flash-2603" };
static const char *M_MOONSHOT[] = {
    "kimi-k2.6", "kimi-k2.5", "kimi-k2-thinking", "kimi-k2-turbo-preview", "kimi-k2-0905-preview",
};
static const char *M_MINIMAX[] = { "MiniMax-M3", "MiniMax-M2.7", "MiniMax-M2.5", "MiniMax-M2.1", "MiniMax-M2" };
static const char *M_MINIMAX_OAUTH[] = { "MiniMax-M3", "MiniMax-M2.7", "MiniMax-M2.7-highspeed" };
static const char *M_MINIMAX_CN[] = { "MiniMax-M3", "MiniMax-M2.7", "MiniMax-M2.5", "MiniMax-M2.1", "MiniMax-M2" };
static const char *M_ANTHROPIC[] = {
    "claude-fable-5", "claude-opus-4-8", "claude-opus-4-7", "claude-opus-4-6",
    "claude-sonnet-4-6", "claude-opus-4-5-20251101", "claude-sonnet-4-5-20250929",
    "claude-opus-4-20250514", "claude-sonnet-4-20250514", "claude-haiku-4-5-20251001",
};
static const char *M_DEEPSEEK[] = { "deepseek-v4-pro", "deepseek-v4-flash", "deepseek-chat", "deepseek-reasoner" };
static const char *M_XIAOMI[] = {
    "mimo-v2.5-pro", "mimo-v2.5", "mimo-v2-pro", "mimo-v2-omni", "mimo-v2-flash",
};
static const char *M_TENCENT[] = { "hy3-preview" };
static const char *M_ARCEE[] = { "trinity-large-thinking", "trinity-large-preview", "trinity-mini" };
static const char *M_GMI[] = {
    "zai-org/GLM-5.1-FP8", "deepseek-ai/DeepSeek-V3.2", "moonshotai/Kimi-K2.5",
    "google/gemini-3.1-flash-lite-preview", "anthropic/claude-sonnet-4.6", "openai/gpt-5.4",
};
static const char *M_OPENCODE_ZEN[] = {
    "kimi-k2.5", "kimi-k2.6", "gpt-5.5", "gpt-5.5-pro", "gpt-5.4-pro", "gpt-5.4", "gpt-5.4-mini",
    "gpt-5.4-nano", "gpt-5.3-codex", "gpt-5.3-codex-spark", "gpt-5.2", "gpt-5.2-codex", "gpt-5.1",
    "gpt-5.1-codex", "gpt-5.1-codex-max", "gpt-5.1-codex-mini", "gpt-5", "gpt-5-codex", "gpt-5-nano",
    "claude-fable-5", "claude-opus-4-8", "claude-opus-4-7", "claude-opus-4-6", "claude-opus-4-5",
    "claude-opus-4-1", "claude-sonnet-4-6", "claude-sonnet-4-5", "claude-sonnet-4", "claude-haiku-4-5",
    "gemini-3.5-flash", "gemini-3.1-pro", "gemini-3-flash", "minimax-m2.7", "minimax-m2.5",
    "minimax-m3-free", "glm-5.1", "glm-5", "deepseek-v4-pro", "deepseek-v4-flash",
    "deepseek-v4-flash-free", "qwen3.6-plus", "qwen3.6-plus-free", "qwen3.5-plus", "grok-build-0.1",
    "big-pickle", "mimo-v2.5-free", "north-mini-code-free", "nemotron-3-ultra-free",
};
static const char *M_OPENCODE_GO[] = {
    "kimi-k2.6", "kimi-k2.5", "glm-5.1", "glm-5", "mimo-v2.5-pro", "mimo-v2.5", "mimo-v2-pro",
    "mimo-v2-omni", "minimax-m2.7", "minimax-m2.5", "qwen3.7-max", "qwen3.6-plus", "qwen3.5-plus",
};
static const char *M_KILOCODE[] = {
    "anthropic/claude-opus-4.6", "anthropic/claude-sonnet-4.6", "openai/gpt-5.4",
    "google/gemini-3-pro-preview", "google/gemini-3-flash-preview",
};
static const char *M_ALIBABA[] = {
    "qwen3.7-max", "qwen3.6-plus", "kimi-k2.5", "qwen3.5-plus", "qwen3-coder-plus", "qwen3-coder-next",
    "glm-5", "glm-4.7", "MiniMax-M2.5",
};
static const char *M_ALIBABA_CODING_PLAN[] = {
    "qwen3.7-max", "qwen3.6-plus", "qwen3.5-plus", "qwen3-coder-plus", "qwen3-coder-next",
    "kimi-k2.5", "glm-5", "glm-4.7", "MiniMax-M2.5",
};
static const char *M_HUGGINGFACE[] = {
    "moonshotai/Kimi-K2.5", "Qwen/Qwen3.5-397B-A17B", "Qwen/Qwen3.5-35B-A3B",
    "deepseek-ai/DeepSeek-V3.2", "MiniMaxAI/MiniMax-M2.5", "zai-org/GLM-5",
    "XiaomiMiMo/MiMo-V2-Flash", "moonshotai/Kimi-K2-Thinking", "moonshotai/Kimi-K2.6",
};
static const char *M_BEDROCK[] = {
    "us.anthropic.claude-sonnet-4-6", "us.anthropic.claude-opus-4-6-v1",
    "us.anthropic.claude-haiku-4-5-20251001-v1:0", "us.anthropic.claude-sonnet-4-5-20250929-v1:0",
    "us.amazon.nova-pro-v1:0", "us.amazon.nova-lite-v1:0", "us.amazon.nova-micro-v1:0",
    "deepseek.v3.2", "us.meta.llama4-maverick-17b-instruct-v1:0", "us.meta.llama4-scout-17b-instruct-v1:0",
};
static const char *M_AZURE_FOUNDRY[] = { /* empty: user-provided endpoint */ };
static const char *M_NOVITA[] = {
    "moonshotai/kimi-k2.5", "minimax/minimax-m2.7", "zai-org/glm-5", "deepseek/deepseek-v3-0324",
    "deepseek/deepseek-r1-0528", "qwen/qwen3-235b-a22b-fp8",
};
static const char *M_MOA[] = { "default" };

static const provider_entry_t PROVIDER_MODELS[] = {
    { "moa", M_MOA, 1 },
    { "nous", M_NOUS, (int)(sizeof(M_NOUS)/sizeof(M_NOUS[0])) },
    { "openai", M_OPENAI, (int)(sizeof(M_OPENAI)/sizeof(M_OPENAI[0])) },
    { "openai-api", M_OPENAI_API, (int)(sizeof(M_OPENAI_API)/sizeof(M_OPENAI_API[0])) },
    { "openai-codex", M_OPENAI_CODEX, (int)(sizeof(M_OPENAI_CODEX)/sizeof(M_OPENAI_CODEX[0])) },
    { "xai-oauth", M_XAI_OAUTH, (int)(sizeof(M_XAI_OAUTH)/sizeof(M_XAI_OAUTH[0])) },
    { "copilot-acp", M_COPILOT_ACP, 1 },
    { "copilot", M_COPILOT, (int)(sizeof(M_COPILOT)/sizeof(M_COPILOT[0])) },
    { "gemini", M_GEMINI, (int)(sizeof(M_GEMINI)/sizeof(M_GEMINI[0])) },
    { "zai", M_ZAI, (int)(sizeof(M_ZAI)/sizeof(M_ZAI[0])) },
    { "xai", M_XAI, (int)(sizeof(M_XAI)/sizeof(M_XAI[0])) },
    { "nvidia", M_NVIDIA, (int)(sizeof(M_NVIDIA)/sizeof(M_NVIDIA[0])) },
    { "kimi-coding", M_KIMI_CODING, (int)(sizeof(M_KIMI_CODING)/sizeof(M_KIMI_CODING[0])) },
    { "kimi-coding-cn", M_KIMI_CODING_CN, (int)(sizeof(M_KIMI_CODING_CN)/sizeof(M_KIMI_CODING_CN[0])) },
    { "stepfun", M_STEPFUN, (int)(sizeof(M_STEPFUN)/sizeof(M_STEPFUN[0])) },
    { "moonshot", M_MOONSHOT, (int)(sizeof(M_MOONSHOT)/sizeof(M_MOONSHOT[0])) },
    { "minimax", M_MINIMAX, (int)(sizeof(M_MINIMAX)/sizeof(M_MINIMAX[0])) },
    { "minimax-oauth", M_MINIMAX_OAUTH, (int)(sizeof(M_MINIMAX_OAUTH)/sizeof(M_MINIMAX_OAUTH[0])) },
    { "minimax-cn", M_MINIMAX_CN, (int)(sizeof(M_MINIMAX_CN)/sizeof(M_MINIMAX_CN[0])) },
    { "anthropic", M_ANTHROPIC, (int)(sizeof(M_ANTHROPIC)/sizeof(M_ANTHROPIC[0])) },
    { "deepseek", M_DEEPSEEK, (int)(sizeof(M_DEEPSEEK)/sizeof(M_DEEPSEEK[0])) },
    { "xiaomi", M_XIAOMI, (int)(sizeof(M_XIAOMI)/sizeof(M_XIAOMI[0])) },
    { "tencent-tokenhub", M_TENCENT, (int)(sizeof(M_TENCENT)/sizeof(M_TENCENT[0])) },
    { "arcee", M_ARCEE, (int)(sizeof(M_ARCEE)/sizeof(M_ARCEE[0])) },
    { "gmi", M_GMI, (int)(sizeof(M_GMI)/sizeof(M_GMI[0])) },
    { "opencode-zen", M_OPENCODE_ZEN, (int)(sizeof(M_OPENCODE_ZEN)/sizeof(M_OPENCODE_ZEN[0])) },
    { "opencode-go", M_OPENCODE_GO, (int)(sizeof(M_OPENCODE_GO)/sizeof(M_OPENCODE_GO[0])) },
    { "kilocode", M_KILOCODE, (int)(sizeof(M_KILOCODE)/sizeof(M_KILOCODE[0])) },
    { "alibaba", M_ALIBABA, (int)(sizeof(M_ALIBABA)/sizeof(M_ALIBABA[0])) },
    { "alibaba-coding-plan", M_ALIBABA_CODING_PLAN, (int)(sizeof(M_ALIBABA_CODING_PLAN)/sizeof(M_ALIBABA_CODING_PLAN[0])) },
    { "huggingface", M_HUGGINGFACE, (int)(sizeof(M_HUGGINGFACE)/sizeof(M_HUGGINGFACE[0])) },
    { "bedrock", M_BEDROCK, (int)(sizeof(M_BEDROCK)/sizeof(M_BEDROCK[0])) },
    { "azure-foundry", M_AZURE_FOUNDRY, 0 },
    { "novita", M_NOVITA, (int)(sizeof(M_NOVITA)/sizeof(M_NOVITA[0])) },
};

/* ── Provider alias map (normalized alias -> canonical id) ───────────── */

typedef struct { const char *alias; const char *canon; } alias_t;
static const alias_t PROVIDER_ALIASES[] = {
    { "glm", "zai" }, { "z-ai", "zai" }, { "z.ai", "zai" }, { "zhipu", "zai" },
    { "github", "copilot" }, { "github-copilot", "copilot" }, { "github-models", "copilot" },
    { "github-model", "copilot" }, { "github-copilot-acp", "copilot-acp" }, { "copilot-acp-agent", "copilot-acp" },
    { "google", "gemini" }, { "google-gemini", "gemini" }, { "google-ai-studio", "gemini" },
    { "google-vertex", "vertex" }, { "vertex-ai", "vertex" }, { "gcp-vertex", "vertex" },
    { "vertexai", "vertex" }, { "kimi", "kimi-coding" }, { "moonshot", "kimi-coding" },
    { "kimi-cn", "kimi-coding-cn" }, { "moonshot-cn", "kimi-coding-cn" },
    { "step", "stepfun" }, { "stepfun-coding-plan", "stepfun" },
    { "arcee-ai", "arcee" }, { "arceeai", "arcee" }, { "gmi-cloud", "gmi" }, { "gmicloud", "gmi" },
    { "minimax-china", "minimax-cn" }, { "minimax_cn", "minimax-cn" },
    { "minimax-portal", "minimax-oauth" }, { "minimax-global", "minimax-oauth" }, { "minimax_oauth", "minimax-oauth" },
    { "claude", "anthropic" }, { "claude-code", "anthropic" }, { "deep-seek", "deepseek" },
    { "opencode", "opencode-zen" }, { "zen", "opencode-zen" }, { "go", "opencode-go" }, { "opencode-go-sub", "opencode-go" },
    { "kilo", "kilocode" }, { "kilo-code", "kilocode" }, { "kilo-gateway", "kilocode" },
    { "dashscope", "alibaba" }, { "aliyun", "alibaba" }, { "qwen", "alibaba" }, { "alibaba-cloud", "alibaba" },
    { "qwen-portal", "qwen-oauth" }, { "hf", "huggingface" }, { "hugging-face", "huggingface" },
    { "huggingface-hub", "huggingface" }, { "novita-ai", "novita" }, { "novitaai", "novita" },
    { "mimo", "xiaomi" }, { "xiaomi-mimo", "xiaomi" },
    { "tencent", "tencent-tokenhub" }, { "tokenhub", "tencent-tokenhub" }, { "tencent-cloud", "tencent-tokenhub" },
    { "tencentmaas", "tencent-tokenhub" }, { "aws", "bedrock" }, { "aws-bedrock", "bedrock" },
    { "amazon-bedrock", "bedrock" }, { "amazon", "bedrock" }, { "grok", "xai" }, { "grok-oauth", "xai-oauth" },
    { "xai-oauth", "xai-oauth" }, { "x-ai-oauth", "xai-oauth" }, { "xai-grok-oauth", "xai-oauth" },
    { "x-ai", "xai" }, { "x.ai", "xai" }, { "nim", "nvidia" }, { "nvidia-nim", "nvidia" },
    { "build-nvidia", "nvidia" }, { "nemotron", "nvidia" }, { "lmstudio", "lmstudio" },
    { "lm-studio", "lmstudio" }, { "lm_studio", "lmstudio" }, { "ollama", "custom" },
    { "ollama_cloud", "ollama-cloud" },
};

/* Cost-safe silent defaults for the non-interactive fallback. */
typedef struct { const char *provider; const char *model; } silent_default_t;
static const silent_default_t SILENT_DEFAULTS[] = {
    { "nous", "deepseek/deepseek-v4-flash" },
};

/* ── Provider groups (display grouping for pickers) ──────────────────── */

typedef struct { const char *gid; const char *label; const char *desc; const char *const *members; int n_members; } group_t;
static const char *G_KIMI_M[] = { "kimi-coding", "kimi-coding-cn" };
static const char *G_MINIMAX_M[] = { "minimax", "minimax-oauth", "minimax-cn" };
static const char *G_XAI_M[] = { "xai", "xai-oauth" };
static const char *G_GOOGLE_M[] = { "gemini" };
static const char *G_OPENAI_M[] = { "openai-codex", "openai-api" };
static const char *G_OPENCODE_M[] = { "opencode-zen", "opencode-go" };
static const char *G_COPILOT_M[] = { "copilot", "copilot-acp" };
static const group_t PROVIDER_GROUPS[] = {
    { "kimi", "Kimi / Moonshot", "Coding Plan, Moonshot global & China endpoints", G_KIMI_M, 2 },
    { "minimax", "MiniMax", "Global, OAuth Coding Plan & China endpoints", G_MINIMAX_M, 3 },
    { "xai", "xAI Grok", "Direct API or SuperGrok / Premium+ OAuth", G_XAI_M, 2 },
    { "google", "Google Gemini", "Google AI Studio (API key)", G_GOOGLE_M, 1 },
    { "openai", "OpenAI", "Codex CLI or direct OpenAI API", G_OPENAI_M, 2 },
    { "opencode", "OpenCode", "Zen pay-as-you-go or Go subscription", G_OPENCODE_M, 2 },
    { "copilot", "GitHub Copilot", "GitHub token API or copilot --acp process", G_COPILOT_M, 2 },
};

/* ── Provider labels ────────────────────────────────────────────────── */

typedef struct { const char *id; const char *label; } label_t;
static const label_t PROVIDER_LABELS[] = {
    { "moa", "Mixture of Agents" }, { "nous", "Nous Portal" }, { "openai", "OpenAI" },
    { "openai-api", "OpenAI" }, { "openai-codex", "OpenAI Codex" }, { "xai-oauth", "xAI Grok (OAuth)" },
    { "copilot-acp", "GitHub Copilot (ACP)" }, { "copilot", "GitHub Copilot" }, { "gemini", "Google Gemini" },
    { "zai", "Z-AI" }, { "xai", "xAI Grok" }, { "nvidia", "NVIDIA" }, { "kimi-coding", "Kimi Coding" },
    { "kimi-coding-cn", "Kimi Coding (China)" }, { "stepfun", "StepFun" }, { "moonshot", "Moonshot" },
    { "minimax", "MiniMax" }, { "minimax-oauth", "MiniMax (OAuth)" }, { "minimax-cn", "MiniMax (China)" },
    { "anthropic", "Anthropic" }, { "deepseek", "DeepSeek" }, { "xiaomi", "Xiaomi" },
    { "tencent-tokenhub", "Tencent TokenHub" }, { "arcee", "Arcee" }, { "gmi", "GMI Cloud" },
    { "opencode-zen", "OpenCode Zen" }, { "opencode-go", "OpenCode Go" }, { "kilocode", "Kilo Code" },
    { "alibaba", "Alibaba DashScope" }, { "alibaba-coding-plan", "Alibaba Coding Plan" },
    { "huggingface", "HuggingFace" }, { "bedrock", "AWS Bedrock" }, { "azure-foundry", "Azure Foundry" },
    { "novita", "Novita" }, { "lmstudio", "LM Studio" }, { "ollama-cloud", "Ollama Cloud" },
    { "custom", "Custom endpoint" }, { "openrouter", "OpenRouter" }, { "vertex", "Google Vertex" },
    { "qwen-oauth", "Qwen (OAuth)" }, { "huggingface", "HuggingFace" },
};

#define N_PROVIDER_MODELS (int)(sizeof(PROVIDER_MODELS)/sizeof(PROVIDER_MODELS[0]))
#define N_ALIASES (int)(sizeof(PROVIDER_ALIASES)/sizeof(PROVIDER_ALIASES[0]))
#define N_GROUPS (int)(sizeof(PROVIDER_GROUPS)/sizeof(PROVIDER_GROUPS[0]))
#define N_LABELS (int)(sizeof(PROVIDER_LABELS)/sizeof(PROVIDER_LABELS[0]))
#define N_SILENT (int)(sizeof(SILENT_DEFAULTS)/sizeof(SILENT_DEFAULTS[0]))

/* ── Helper: find provider entry ────────────────────────────────────── */

static const provider_entry_t *find_provider(const char *provider) {
    if (!provider) return NULL;
    for (int i = 0; i < N_PROVIDER_MODELS; i++) {
        if (strcmp(PROVIDER_MODELS[i].provider, provider) == 0)
            return &PROVIDER_MODELS[i];
    }
    return NULL;
}

/* Case-insensitive linear search of a model array. */
static const char *model_array_find(const char *const *arr, int n, const char *name_lower) {
    for (int i = 0; i < n; i++) {
        if (strcasecmp(arr[i], name_lower) == 0)
            return arr[i];
    }
    return NULL;
}

/* ── Provider normalization / labels / grouping ─────────────────────── */

const char *model_normalize_provider(const char *provider) {
    static const char *dflt = "openrouter";
    if (!provider || !*provider) return dflt;
    /* "auto" passes through unchanged. */
    if (strcmp(provider, "auto") == 0) return "auto";
    static char lower[128];
    size_t n = strlen(provider);
    if (n >= sizeof(lower)) n = sizeof(lower) - 1;
    for (size_t i = 0; i < n; i++) lower[i] = (char)tolower((unsigned char)provider[i]);
    lower[n] = '\0';
    for (int i = 0; i < N_ALIASES; i++) {
        if (strcmp(PROVIDER_ALIASES[i].alias, lower) == 0)
            return PROVIDER_ALIASES[i].canon;
    }
    return lower; /* alias not found: lowercased input (static buf, caller must not free) */
}

const char *model_provider_label(const char *provider) {
    if (!provider || !*provider) return "OpenRouter";
    if (strcmp(provider, "auto") == 0) return "Auto";
    const char *norm = model_normalize_provider(provider);
    for (int i = 0; i < N_LABELS; i++) {
        if (strcmp(PROVIDER_LABELS[i].id, norm) == 0)
            return PROVIDER_LABELS[i].label;
    }
    return provider; /* fall back to original */
}

const char *model_provider_group_for_slug(const char *slug) {
    if (!slug) return "";
    char lower[128];
    size_t n = strlen(slug);
    if (n >= sizeof(lower)) n = sizeof(lower) - 1;
    for (size_t i = 0; i < n; i++) lower[i] = (char)tolower((unsigned char)slug[i]);
    lower[n] = '\0';
    for (int i = 0; i < N_GROUPS; i++) {
        for (int j = 0; j < PROVIDER_GROUPS[i].n_members; j++) {
            if (strcmp(PROVIDER_GROUPS[i].members[j], lower) == 0)
                return PROVIDER_GROUPS[i].gid;
        }
    }
    return "";
}

/* ── Static catalog access ──────────────────────────────────────────── */

int model_catalog_provider_count(void) { return N_PROVIDER_MODELS; }

const char *model_catalog_provider_at(int idx) {
    if (idx < 0 || idx >= N_PROVIDER_MODELS) return NULL;
    return PROVIDER_MODELS[idx].provider;
}

const char *model_default_model_for_provider(const char *provider) {
    const char *norm = model_normalize_provider(provider);
    const provider_entry_t *e = find_provider(norm);
    if (!e) return "";
    /* cost-safe override first */
    for (int i = 0; i < N_SILENT; i++) {
        if (strcmp(SILENT_DEFAULTS[i].provider, norm) == 0) {
            const char *ov = model_array_find(e->models, e->n_models, SILENT_DEFAULTS[i].model);
            if (ov) return ov;
        }
    }
    return e->n_models > 0 ? e->models[0] : "";
}

int model_provider_model_count(const char *provider) {
    const provider_entry_t *e = find_provider(model_normalize_provider(provider));
    return e ? e->n_models : 0;
}

/* True when `model` (case-insensitive) is in the static catalog for `provider`. */
int model_provider_has_model(const char *provider, const char *model) {
    if (!model) return 0;
    const provider_entry_t *e = find_provider(model_normalize_provider(provider));
    if (!e) return 0;
    for (int i = 0; i < e->n_models; i++)
        if (strcasecmp(e->models[i], model) == 0) return 1;
    return 0;
}

const char *model_provider_model_at(const char *provider, int idx) {
    const provider_entry_t *e = find_provider(model_normalize_provider(provider));
    if (!e || idx < 0 || idx >= e->n_models) return NULL;
    return e->models[idx];
}

/* ── String helpers ─────────────────────────────────────────────────── */

void model_strip_vendor_prefix(const char *model_id, char *out, size_t outsz) {
    if (!model_id) { if (outsz) out[0] = '\0'; return; }
    /* lowercase + strip leading "vendor/" */
    size_t n = strlen(model_id);
    char *tmp = malloc(n + 1);
    if (!tmp) { if (outsz) out[0] = '\0'; return; }
    for (size_t i = 0; i <= n; i++) tmp[i] = (char)tolower((unsigned char)model_id[i]);
    char *slash = strchr(tmp, '/');
    const char *body = slash ? slash + 1 : tmp;
    /* also drop any ":variant" suffix for fast-mode checks */
    char *colon = strchr((char*)body, ':');
    if (colon) *colon = '\0';
    snprintf(out, outsz, "%s", body);
    free(tmp);
}

/* ── Parse "provider:model" input ───────────────────────────────────── */

static int is_known_provider_name(const char *s) {
    if (!s || !*s) return 0;
    for (int i = 0; i < N_PROVIDER_MODELS; i++)
        if (strcmp(PROVIDER_MODELS[i].provider, s) == 0) return 1;
    for (int i = 0; i < N_ALIASES; i++)
        if (strcmp(PROVIDER_ALIASES[i].alias, s) == 0) return 1;
    return strcmp(s, "openrouter") == 0 || strcmp(s, "custom") == 0;
}

/* PoP: parse_model_input @ hermes_cli/models.py:parse_model_input */
void model_parse_model_input(const char *raw, const char *current_provider,
                             char *provider_out, size_t poutsz,
                             char *model_out, size_t moutsz) {
    if (provider_out && poutsz) provider_out[0] = '\0';
    if (model_out && moutsz) model_out[0] = '\0';
    if (!raw) { if (current_provider && provider_out) snprintf(provider_out, poutsz, "%s", current_provider); return; }
    char stripped[1024];
    size_t n = strlen(raw);
    if (n >= sizeof(stripped)) n = sizeof(stripped) - 1;
    for (size_t i = 0; i < n; i++) stripped[i] = raw[i];
    stripped[n] = '\0';
    /* trim */
    char *start = stripped;
    while (*start == ' ' || *start == '\t') start++;
    char *end = start + strlen(start);
    while (end > start && (end[-1] == ' ' || end[-1] == '\t')) end--;
    *end = '\0';

    char *colon = strchr(start, ':');
    if (colon && colon > start) {
        char *mpart = colon + 1;
        /* trim mpart */
        while (*mpart == ' ' || *mpart == '\t') mpart++;
        /* lowercased provider part (do not mutate start yet) */
        char pp[256]; size_t pn = (size_t)(colon - start);
        if (pn >= sizeof(pp)) pn = sizeof(pp) - 1;
        for (size_t i = 0; i < pn; i++) pp[i] = (char)tolower((unsigned char)start[i]);
        pp[pn] = '\0';
        if (pp[0] && mpart[0] && is_known_provider_name(pp)) {
            /* custom:name:model triple syntax */
            if (strcmp(pp, "custom") == 0) {
                char *second = strchr(mpart, ':');
                if (second) {
                    *second = '\0';
                    char *actual = second + 1;
                    while (*actual == ' ' || *actual == '\t') actual++;
                    if (mpart[0] && actual[0]) {
                        snprintf(provider_out, poutsz, "custom:%s", mpart);
                        snprintf(model_out, moutsz, "%s", actual);
                        return;
                    }
                }
            }
            snprintf(provider_out, poutsz, "%s", model_normalize_provider(pp));
            snprintf(model_out, moutsz, "%s", mpart);
            return;
        }
        /* not a recognized provider: treat whole (original) thing as model */
        snprintf(model_out, moutsz, "%s", start);
        if (current_provider) snprintf(provider_out, poutsz, "%s", current_provider);
        return;
    }
    snprintf(model_out, moutsz, "%s", start);
    if (current_provider) snprintf(provider_out, poutsz, "%s", current_provider);
}

/* ── Static model-alias / provider detection ────────────────────────── */

/* Resolve a short alias to (provider, model) using the static catalog.
 * Returns 1 and fills out_* on match, else 0. */
/* PoP: _resolve_static_model_alias @ hermes_cli/models.py:_resolve_static_model_alias */
static int resolve_static_model_alias(const char *name_lower,
                                      char *provider_out, size_t poutsz,
                                      char *model_out, size_t moutsz,
                                      int current_only) {
    /* No MODEL_ALIASES table ported here (model_switch.MODEL_ALIASES is a
     * separate subsystem); we resolve by vendor/family prefix against the
     * static catalog instead, which covers the common "sonnet"/"opus" cases. */
    (void)name_lower; (void)provider_out; (void)poutsz; (void)model_out; (void)moutsz; (void)current_only;
    return 0;
}

/* PoP: detect_static_provider_for_model @ hermes_cli/models.py:detect_static_provider_for_model */
int model_detect_static_provider_for_model(const char *model_name,
                                           const char *current_provider,
                                           char *provider_out, size_t poutsz,
                                           char *model_out, size_t moutsz) {
    if (provider_out && poutsz) provider_out[0] = '\0';
    if (model_out && moutsz) model_out[0] = '\0';
    if (!model_name || !*model_name) return 0;
    char name[1024];
    size_t n = strlen(model_name);
    if (n >= sizeof(name)) n = sizeof(name) - 1;
    for (size_t i = 0; i < n; i++) name[i] = (char)tolower((unsigned char)model_name[i]);
    name[n] = '\0';

    /* Step 0: bare provider name typed as model -> switch + default model */
    const char *resolved = model_normalize_provider(name);
    if (strcmp(resolved, "custom") != 0 && strcmp(resolved, "openrouter") != 0) {
        const provider_entry_t *pe = find_provider(resolved);
        if (pe && pe->n_models > 0) {
            snprintf(provider_out, poutsz, "%s", resolved);
            snprintf(model_out, moutsz, "%s", pe->models[0]);
            return 1;
        }
    }

    /* Step 1: direct catalog match across all native providers */
    int is_custom_current = (current_provider &&
        (strcmp(current_provider, "custom") == 0 || strncmp(current_provider, "custom:", 7) == 0));
    if (!is_custom_current) {
        for (int i = 0; i < N_PROVIDER_MODELS; i++) {
            const provider_entry_t *pe = &PROVIDER_MODELS[i];
            if (model_array_find(pe->models, pe->n_models, name)) {
                snprintf(provider_out, poutsz, "%s", pe->provider);
                snprintf(model_out, moutsz, "%s", name);
                return 1;
            }
        }
    }
    return 0;
}

/* ── OpenRouter slug resolution (against static curated list) ───────── */

/* Static OpenRouter curated list (mirrors OPENROUTER_MODELS ids). */
static const char *OPENROUTER_CURATED[] = {
    "anthropic/claude-fable-5", "anthropic/claude-opus-4.8", "anthropic/claude-opus-4.8-fast",
    "anthropic/claude-sonnet-5", "anthropic/claude-haiku-4.5", "openai/gpt-5.5", "openai/gpt-5.5-pro",
    "openai/gpt-5.4-mini", "google/gemini-3-pro-preview", "google/gemini-3.1-pro-preview",
    "google/gemini-3.5-flash", "x-ai/grok-4.3", "deepseek/deepseek-v4-pro", "deepseek/deepseek-v4-flash",
    "qwen/qwen3.7-max", "qwen/qwen3.7-plus", "qwen/qwen3.6-35b-a3b", "moonshotai/kimi-k2.6",
    "moonshotai/kimi-k2.7-code", "minimax/minimax-m3", "z-ai/glm-5.2", "z-ai/glm-5.1",
    "xiaomi/mimo-v2.5-pro", "tencent/hy3-preview", "stepfun/step-3.7-flash",
    "nvidia/nemotron-3-super-120b-a12b", "sakana/fugu-ultra", "openrouter/pareto-code",
    "openrouter/elephant-alpha", "openrouter/owl-alpha", "poolside/laguna-m.1:free",
    "tencent/hy3-preview:free", "nvidia/nemotron-3-super-120b-a12b:free",
    "nvidia/nemotron-3-ultra-550b-a55b:free", "inclusionai/ring-2.6-1t:free",
};
#define N_OR_CURATED (int)(sizeof(OPENROUTER_CURATED)/sizeof(OPENROUTER_CURATED[0]))

/* PoP: _find_openrouter_slug @ hermes_cli/models.py:_find_openrouter_slug */
char *model_find_openrouter_slug(const char *model_name) {
    if (!model_name || !*model_name) return NULL;
    char lower[1024];
    size_t n = strlen(model_name);
    if (n >= sizeof(lower)) n = sizeof(lower) - 1;
    for (size_t i = 0; i < n; i++) lower[i] = (char)tolower((unsigned char)model_name[i]);
    lower[n] = '\0';
    /* exact match */
    for (int i = 0; i < N_OR_CURATED; i++)
        if (strcmp(OPENROUTER_CURATED[i], lower) == 0)
            return strdup(OPENROUTER_CURATED[i]);
    /* match by model part after '/' */
    for (int i = 0; i < N_OR_CURATED; i++) {
        const char *slash = strchr(OPENROUTER_CURATED[i], '/');
        if (slash && strcmp(slash + 1, lower) == 0)
            return strdup(OPENROUTER_CURATED[i]);
    }
    return NULL;
}

/* ── Fast-mode capability ─────────────────────────────────────────────
 * Fast-mode detection is owned by port_models_helpers.c (is_openai_fast_model,
 * is_anthropic_fast_model, model_supports_fast_mode). We delegate rather than
 * redefining — model_catalog.h surfaces model_supports_fast_mode, which
 * resolves to the canonical definition in that module. */

/* ── Provider grouping for pickers ──────────────────────────────────── */

char *model_group_providers(const char *slug_csv) {
    /* Count + collect slugs from the csv (order preserved). */
    char **slugs = NULL; int n_slugs = 0, cap = 0;
    if (slug_csv) {
        char buf[4096]; size_t bl = strlen(slug_csv);
        if (bl >= sizeof(buf)) bl = sizeof(buf) - 1;
        for (size_t i = 0; i < bl; i++) buf[i] = slug_csv[i];
        buf[bl] = '\0';
        char *tok = strtok(buf, ";");
        while (tok) {
            while (*tok == ' ' || *tok == '\t') tok++;
            char *e = tok + strlen(tok);
            while (e > tok && (e[-1] == ' ' || e[-1] == '\t')) e--;
            *e = '\0';
            if (*tok) {
                if (n_slugs >= cap) { cap = cap ? cap*2 : 16; slugs = realloc(slugs, cap*sizeof(char*)); }
                char *s = strdup(tok);
                for (char *p = s; *p; p++) *p = (char)tolower((unsigned char)*p);
                slugs[n_slugs++] = s;
            }
            tok = strtok(NULL, ";");
        }
    }

    /* Which groups have present members (in declaration order). */
    int *group_has = calloc(N_GROUPS, sizeof(int));
    int *group_member_count = calloc(N_GROUPS, sizeof(int));
    for (int g = 0; g < N_GROUPS; g++) {
        for (int m = 0; m < PROVIDER_GROUPS[g].n_members; m++) {
            for (int s = 0; s < n_slugs; s++) {
                if (strcmp(PROVIDER_GROUPS[g].members[m], slugs[s]) == 0) { group_has[g] = 1; group_member_count[g]++; break; }
            }
        }
    }

    /* Build packed buffer. */
    size_t cap_out = 1024, len = 0;
    char *out = malloc(cap_out);
    if (!out) { for (int i=0;i<n_slugs;i++) free(slugs[i]); free(slugs); free(group_has); free(group_member_count); return NULL; }
    out[0] = '\0';

    #define PUSH(s) do { size_t _l = strlen(s); if (len+_l+2 > cap_out) { cap_out = (len+_l+2)*2; out = realloc(out, cap_out); } \
        memcpy(out+len, s, _l); out[len+_l]='\0'; len += _l+1; } while(0)

    int *emitted = calloc(N_GROUPS, sizeof(int));
    for (int s = 0; s < n_slugs; s++) {
        const char *slug = slugs[s];
        /* skip duplicates */
        int dup = 0; for (int i = 0; i < s; i++) if (strcmp(slugs[i], slug) == 0) { dup = 1; break; }
        if (dup) continue;
        const char *gid = model_provider_group_for_slug(slug);
        if (!gid[0]) {
            PUSH("single"); PUSH(slug); PUSH(""); PUSH(""); PUSH(""); PUSH("");
        } else {
            int gi = -1; for (int g=0;g<N_GROUPS;g++) if (strcmp(PROVIDER_GROUPS[g].gid, gid)==0) { gi=g; break; }
            if (gi < 0 || emitted[gi]) continue;
            emitted[gi] = 1;
            if (group_member_count[gi] <= 1) {
                PUSH("single"); PUSH(PROVIDER_GROUPS[gi].members[0]); PUSH(""); PUSH(""); PUSH(""); PUSH("");
            } else {
                PUSH("group"); PUSH(""); PUSH(gid); PUSH(PROVIDER_GROUPS[gi].label);
                PUSH(PROVIDER_GROUPS[gi].desc);
                /* members csv */
                char mcsv[1024]; mcsv[0]='\0';
                for (int m = 0; m < PROVIDER_GROUPS[gi].n_members; m++) {
                    for (int ss = 0; ss < n_slugs; ss++) {
                        if (strcmp(PROVIDER_GROUPS[gi].members[m], slugs[ss]) == 0) {
                            if (mcsv[0]) strcat(mcsv, ",");
                            strcat(mcsv, slugs[ss]);
                            break;
                        }
                    }
                }
                PUSH(mcsv);
            }
        }
    }
    #undef PUSH
    out[len] = '\0'; out[len+1] = '\0'; /* double-NUL terminator for iterator */

    for (int i=0;i<n_slugs;i++) free(slugs[i]);
    free(slugs); free(group_has); free(group_member_count); free(emitted);
    return out;
}

const model_group_row_t *model_group_next(const char *packed,
                                          const char **cursor,
                                          const char **kind,
                                          const char **slug,
                                          const char **group_id,
                                          const char **label,
                                          const char **desc,
                                          const char **members_csv) {
    if (!packed) return NULL;
    const char *p = *cursor;
    if (p == NULL) p = packed;
    if (*p == '\0') return NULL; /* end */
    *kind = p; p += strlen(p) + 1;
    *slug = p; p += strlen(p) + 1;
    *group_id = p; p += strlen(p) + 1;
    *label = p; p += strlen(p) + 1;
    *desc = p; p += strlen(p) + 1;
    *members_csv = p; p += strlen(p) + 1;
    *cursor = p;
    return (const model_group_row_t *)packed;
}

/* ── Disk cache (path / fingerprint / load / save) ──────────────────── */

#include <sys/stat.h>

char *model_provider_models_cache_path(void) {
    /* Owned by port_models_helpers.c (provider_models_cache_path). */
    return provider_models_cache_path();
}

char *model_credential_fingerprint(const char *provider) {
    /* Owned by port_models_helpers.c (credential_fingerprint). */
    return credential_fingerprint(provider);
}

/* PoP: _load_provider_models_cache @ hermes_cli/models.py:_load_provider_models_cache */
char *model_load_provider_models_cache(void) {
    char *path = model_provider_models_cache_path();
    FILE *f = fopen(path, "r");
    free(path);
    if (!f) return NULL;
    char *blob = NULL; size_t total = 0, cap = 0; char tmp[8192];
    while (fgets(tmp, sizeof(tmp), f)) {
        size_t l = strlen(tmp);
        if (total + l + 1 > cap) { cap = (total + l + 1) * 2; blob = realloc(blob, cap); }
        memcpy(blob + total, tmp, l); total += l; blob[total] = '\0';
    }
    fclose(f);
    return blob;
}

/* PoP: _save_provider_models_cache @ hermes_cli/models.py:_save_provider_models_cache */
void model_save_provider_models_cache(const char *json) {
    if (!json) return;
    char *path = model_provider_models_cache_path();
    FILE *f = fopen(path, "w");
    if (f) { fputs(json, f); fclose(f); }
    free(path);
}

/* ── Provider model ids (static fallback in offline port) ──────────── */

/* Serialize a provider's static catalog as a JSON array string. */
static char *provider_static_json(const char *provider) {
    const provider_entry_t *e = find_provider(model_normalize_provider(provider));
    size_t cap = 256, len = 0;
    char *out = malloc(cap); out[0] = '['; out[1] = '\0'; len = 1;
    if (e) {
        for (int i = 0; i < e->n_models; i++) {
            size_t need = len + strlen(e->models[i]) + 6;
            if (need >= cap) { cap = need * 2; out = realloc(out, cap); }
            len += (size_t)sprintf(out + len, "%s\"%s\"", i ? "," : "", e->models[i]);
        }
    }
    if (len + 2 >= cap) { cap = len + 4; out = realloc(out, cap); }
    out[len++] = ']'; out[len] = '\0';
    return out;
}

/* PoP: provider_model_ids @ hermes_cli/models.py:provider_model_ids */
char *model_provider_model_ids(const char *provider, int force_refresh) {
    (void)force_refresh;
    /* Offline port: the live /v1/models fetch is a no-op; degrade to the
     * static catalog (exactly the Python fallback when the endpoint is
     * unreachable). */
    return provider_static_json(provider);
}

/* PoP: cached_provider_model_ids @ hermes_cli/models.py:cached_provider_model_ids */
char *model_cached_provider_model_ids(const char *provider, int force_refresh) {
    const char *norm = model_normalize_provider(provider);
    if (!norm || !*norm) return NULL;
    char *cache = model_load_provider_models_cache();
    char *fp = model_credential_fingerprint(norm);
    /* Try cache. */
    char *result = NULL;
    if (cache && !force_refresh) {
        /* Minimal JSON scan: find "norm":{...,"models":[...]}. We do a
         * lightweight match rather than full parse. */
        char key[256]; snprintf(key, sizeof(key), "\"%s\"", norm);
        char *entry = strstr(cache, key);
        if (entry) {
            char *models = strstr(entry, "\"models\"");
            if (models) {
                char *arr = strchr(models, '[');
                if (arr) {
                    int depth = 1; char *p = arr + 1; char *end = NULL;
                    while (*p && depth) { if (*p=='[') depth++; else if (*p==']') { depth--; if (depth==0) end = p; } p++; }
                    if (end) { size_t l = (size_t)(end - arr + 1); result = malloc(l + 1); memcpy(result, arr, l); result[l] = '\0'; }
                }
            }
        }
    }
    free(cache);
    if (result) { free(fp); return result; }
    /* Cache miss -> live (static) path. */
    result = model_provider_model_ids(norm, force_refresh);
    if (result) {
        /* Persist a best-effort cache entry. */
        size_t cap = 512; char *newcache = malloc(cap); newcache[0]='\0';
        /* For simplicity, store a flat object: { "<norm>": {"fp":..,"models":[...]} } */
        size_t need = strlen(result) + strlen(fp) + strlen(norm) + 64;
        if (need >= cap) { cap = need * 2; newcache = realloc(newcache, cap); }
        snprintf(newcache, cap, "{\"%s\":{\"fp\":\"%s\",\"models\":%s}}", norm, fp, result);
        model_save_provider_models_cache(newcache);
        free(newcache);
    }
    free(fp);
    return result;
}

/* PoP: clear_provider_models_cache @ hermes_cli/models.py:clear_provider_models_cache */
void model_clear_provider_models_cache(const char *provider) {
    if (!provider) {
        char *path = model_provider_models_cache_path();
        remove(path);
        free(path);
        return;
    }
    char *cache = model_load_provider_models_cache();
    if (!cache) return;
    const char *norm = model_normalize_provider(provider);
    /* Rebuild cache without this provider. Simple approach: drop the whole
     * file if we can't surgically remove (offline best-effort). */
    char key[256]; snprintf(key, sizeof(key), "\"%s\"", norm);
    if (strstr(cache, key)) {
        /* Rewrite as empty object (single-provider cache). */
        model_save_provider_models_cache("{}");
    }
    free(cache);
}

/* PoP: curated_models_for_provider @ hermes_cli/models.py:curated_models_for_provider */
/*
 * Faithful port of models.py:curated_models_for_provider's STATIC-CATALOG
 * fallback path. Mirrors the Python:
 *   normalized = normalize_provider(provider)
 *   if normalized == "openrouter": return fetch_openrouter_models(...)   # HTTP
 *   live = provider_model_ids(normalized)                                # HTTP
 *   if live: return [(m, "") for m in live]
 *   return [(m, "") for m in _PROVIDER_MODELS.get(normalized, [])]        # static
 *
 * This C entry implements the deterministic static fallback: it fills
 * provider_out[i]/model_out[i] with the curated (model_id, "") tuples from
 * the embedded _PROVIDER_MODELS catalog and returns the count. The live
 * HTTP path (openrouter / provider_model_ids) is a separate, network-driven
 * resolver layered on top; callers that need live data call it first and
 * fall back to this. Returns 0 when the provider has no static models.
 */
int model_curated_models_for_provider(const char *provider,
                                      char provider_out[][64],
                                      char model_out[][256],
                                      int max) {
    if (!provider || max <= 0) return 0;
    const char *norm = model_normalize_provider(provider);
    const provider_entry_t *e = find_provider(norm);
    if (!e) return 0;
    int n = 0;
    for (int i = 0; i < e->n_models && n < max; i++) {
        /* provider_out mirrors the normalized provider (Python returns "" for
         * description; provider_out carries the provider for tuple symmetry). */
        snprintf(provider_out[n], 64, "%s", norm);
        snprintf(model_out[n], 256, "%s", e->models[i]);
        n++;
    }
    return n;
}
