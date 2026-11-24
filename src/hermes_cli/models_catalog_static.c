/*
 * models_catalog_static.c - C11 port of upstream
 * hermes_cli/models_catalog_static.py (gate w30_modelscata_4ee086).
 *
 * PoP: w30_modelscata_4ee086_constants @ hermes_cli/models_catalog_static.py:<module tables>
 * PoP: w30_modelscata_4ee086_xai_promote_top @ hermes_cli/models_catalog_static.py:_xai_promote_top
 * PoP: w30_modelscata_4ee086_xai_merge_curated_extras @ hermes_cli/models_catalog_static.py:_xai_merge_curated_extras
 * PoP: w30_modelscata_4ee086_xai_finalize_catalog @ hermes_cli/models_catalog_static.py:_xai_finalize_catalog
 * PoP: w30_modelscata_4ee086_provider_group_for_slug @ hermes_cli/models_catalog_static.py:provider_group_for_slug
 * PoP: w30_modelscata_4ee086_group_providers @ hermes_cli/models_catalog_static.py:group_providers
 * PoP: w30_modelscata_4ee086_plugin_provider_enters_picker @ hermes_cli/models_catalog_static.py:_plugin_provider_enters_picker
 * PoP: w30_modelscata_4ee086_provider_models @ hermes_cli/models_catalog_static.py:_PROVIDER_MODELS[slug]
 * PoP: w30_modelscata_4ee086_provider_label @ hermes_cli/models_catalog_static.py:_PROVIDER_LABELS[slug]
 * PoP: w30_modelscata_4ee086_provider_alias @ hermes_cli/models_catalog_static.py:_PROVIDER_ALIASES[key]
 * PoP: w30_modelscata_4ee086_canonical_provider_row @ hermes_cli/models_catalog_static.py:CANONICAL_PROVIDERS[slug]
 * PoP: w30_modelscata_4ee086_group_members @ hermes_cli/models_catalog_static.py:PROVIDER_GROUPS[gid]
 *
 * The module is data: curated per-provider model ids, the canonical provider
 * registry, the display-only group fold, the alias table and the capability
 * sets that the picker and the router branch on. Everything here is a pure
 * function of a value and those frozen tables.
 *
 * SEAMS - kept in the caller, never entered here:
 *   - _xai_curated_models(): the models.dev disk cache under $HERMES_HOME
 *     decides the xai / xai-oauth rows. This file owns the static floor they
 *     fall back to (xai_static_fallback) and the finaliser that shapes it.
 *   - _codex_curated_models(): hermes_cli.codex_models owns the openai-codex
 *     row; it is therefore reported in dynamic_model_rows, not in the table.
 *   - sync_plugin_provider_catalog() / _CatalogAliases: the plugin provider
 *     registry extends CANONICAL_PROVIDERS, _PROVIDER_LABELS, PROVIDER_GROUPS,
 *     _SLUG_TO_GROUP, _AGGREGATOR_PROVIDERS and the alias table at import.
 *     The built-in tables those mutations start from are what is ported.
 *   - listed_canonical_providers(): needs hermes_cli.models + providers.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w30_modelscata_4ee086.h"

/* ---- curated model snapshots ------------------------------------------- */

static const char *const MC_OPENROUTER[] = {
    "anthropic/claude-fable-5.2", "anthropic/claude-fable-5.1", "anthropic/claude-fable-5", "anthropic/claude-opus-5.5", "anthropic/claude-opus-5", "anthropic/claude-opus-5-fast",
    "anthropic/claude-opus-4.8", "anthropic/claude-opus-4.8-fast", "anthropic/claude-sonnet-5.5", "anthropic/claude-sonnet-5", "anthropic/claude-haiku-5.5", "anthropic/claude-haiku-4.5",
    "openai/gpt-6-astra", "openai/gpt-6-astra-fast", "openai/gpt-6-astra-flex", "openai/gpt-6-astra-pro", "openai/gpt-6-astra-pro-fast", "openai/gpt-6-astra-pro-flex",
    "openai/gpt-6.1-sol", "openai/gpt-6.1-sol-pro", "openai/gpt-6.1-terra", "openai/gpt-6-sol", "openai/gpt-6-sol-pro", "openai/gpt-6-luna",
    "openai/gpt-6-luna-pro", "openai/gpt-5.5", "openai/gpt-5.5-pro", "openai/gpt-5.4-mini", "google/gemini-4-argon", "google/gemini-3.1-pro-preview",
    "google/gemini-3.8-flash", "google/gemini-3.7-flash", "x-ai/grok-4.7", "x-ai/grok-4.6", "deepseek/deepseek-v4-pro", "deepseek/deepseek-v4-pro-0813",
    "deepseek/deepseek-v4.1-flash", "deepseek/deepseek-v4.1-flash-1017", "deepseek/deepseek-v4-flash-0731", "qwen/qwen3.8-max-0902", "qwen/qwen3.8-flash", "moonshotai/kimi-k3",
    "minimax/minimax-m3", "z-ai/glm-5.3", "z-ai/glm-5.3-flash", "z-ai/glm-5.3-flashx", "z-ai/glm-5.2", "xiaomi/mimo-v2.6-pro",
    "xiaomi/mimo-v2.6-flash", "xiaomi/mimo-v2.6-pro-ultraspeed", "xiaomi/mimo-v2.5-pro", "tencent/hy4-preview", "tencent/hy3", "stepfun/step-3.7-flash",
    "nvidia/nemotron-3-super-120b-a12b", "meta/muse-spark-1.2", "meta/muse-spark-1.2-contributor", "meta/muse-spark-1.3", "meta/muse-spark-1.3-contributor", "sakana/fugu-ultra",
    "upstage/solar-pro4", "upstage/solar-mini4", "openrouter/pareto-code", "thinkingmachines/inkling:free", "thinkingmachines/inkling-small:free", "minimax/minimax-m3:free",
    "z-ai/glm-5.2:free", "poolside/laguna-s-2.1:free", "poolside/laguna-xs-2.1:free", "nvidia/nemotron-3-super-120b-a12b:free", "nvidia/nemotron-3-ultra-550b-a55b:free", "inclusionai/ling-3.0-flash:free",
    "nvidia/nemotron-3.5-lightning:free", "stealth/union-alpha", "stealth/space-bunny-alpha",
};
#define MC_OPENROUTER_N (sizeof MC_OPENROUTER / sizeof MC_OPENROUTER[0])

static const char *const MC_OPENROUTER_DESC[] = {
    "", "", "", "", "", "2x price, higher output speed",
    "", "2x price, higher output speed", "", "", "", "",
    "", "2x price, priority tier", "0.5x price, flex tier", "", "2x price, priority tier", "0.5x price, flex tier",
    "", "", "", "", "", "",
    "", "", "", "", "", "",
    "", "", "", "", "", "dated snapshot of v4-pro",
    "", "dated snapshot of v4.1-flash", "dated snapshot of v4-flash", "", "", "recommended",
    "", "default", "", "high-speed tier of glm-5.3-flash", "", "",
    "", "", "", "", "", "",
    "", "", "", "", "", "",
    "", "", "auto-routes to cheapest coder meeting openrouter.min_coding_score", "free", "free", "free",
    "free", "free", "free", "free", "free", "free",
    "free", "free, stealth model", "free, stealth model",
};
#define MC_OPENROUTER_DESC_N (sizeof MC_OPENROUTER_DESC / sizeof MC_OPENROUTER_DESC[0])

static const char *const MC_VERCEL[] = {
    "moonshotai/kimi-k2.6", "alibaba/qwen3.6-plus", "zai/glm-5.1", "minimax/minimax-m2.7", "anthropic/claude-sonnet-4.6", "anthropic/claude-opus-4.7",
    "anthropic/claude-opus-4.6", "anthropic/claude-haiku-4.5", "openai/gpt-5.4", "openai/gpt-5.4-mini", "openai/gpt-5.3-codex", "google/gemini-3.1-pro-preview",
    "google/gemini-3-flash", "google/gemini-3.1-flash-lite-preview", "xai/grok-4.20-reasoning",
};
#define MC_VERCEL_N (sizeof MC_VERCEL / sizeof MC_VERCEL[0])

static const char *const MC_VERCEL_DESC[] = {
    "recommended", "", "", "", "", "",
    "", "", "", "", "", "",
    "", "", "",
};
#define MC_VERCEL_DESC_N (sizeof MC_VERCEL_DESC / sizeof MC_VERCEL_DESC[0])

static const char *const MC_OPENROUTER_ONLY[] = {
    "anthropic/claude-opus-4.8-fast", "anthropic/claude-opus-5-fast", "meta/muse-spark-1.2-contributor", "meta/muse-spark-1.3-contributor", "openrouter/pareto-code", "stealth/union-alpha",
};
#define MC_OPENROUTER_ONLY_N (sizeof MC_OPENROUTER_ONLY / sizeof MC_OPENROUTER_ONLY[0])

static const char *const MC_XAI_STATIC[] = {
    "grok-4.6", "grok-build-0.1", "grok-4.5", "grok-4.3", "grok-4.20-0309-reasoning", "grok-4.20-0309-non-reasoning",
    "grok-4.20-multi-agent-0309",
};
#define MC_XAI_STATIC_N (sizeof MC_XAI_STATIC / sizeof MC_XAI_STATIC[0])

static const char *const MC_XAI_EXTRAS[] = {
    "grok-4.6", "grok-4.5", "grok-composer-2.5-fast",
};
#define MC_XAI_EXTRAS_N (sizeof MC_XAI_EXTRAS / sizeof MC_XAI_EXTRAS[0])

static const char *const MC_OPENAI_CHAT[] = {
    "gpt-5.4", "gpt-5.4-mini", "gpt-5-mini", "gpt-5.3-codex", "gpt-5.2-codex", "gpt-4.1",
    "gpt-4o", "gpt-4o-mini",
};
#define MC_OPENAI_CHAT_N (sizeof MC_OPENAI_CHAT / sizeof MC_OPENAI_CHAT[0])

static const char *const MC_MINIMAX[] = {
    "MiniMax-M3", "MiniMax-M2.7", "MiniMax-M2.5", "MiniMax-M2.1", "MiniMax-M2",
};
#define MC_MINIMAX_N (sizeof MC_MINIMAX / sizeof MC_MINIMAX[0])

static const char *const MC_TENCENT[] = {
    "hy4-preview", "hy3", "hy3-preview",
};
#define MC_TENCENT_N (sizeof MC_TENCENT / sizeof MC_TENCENT[0])

static const char *const MC_ALIBABA[] = {
    "qwen3.8-max", "qwen3.7-max", "qwen3.7-plus", "qwen3.6-plus", "qwen3.6-flash", "kimi-k2.5",
    "qwen3.5-plus", "qwen3-coder-plus", "qwen3-coder-next", "glm-5.2", "glm-5", "glm-4.7",
    "deepseek-v4-pro", "deepseek-v4-flash-0731", "MiniMax-M2.5",
};
#define MC_ALIBABA_N (sizeof MC_ALIBABA / sizeof MC_ALIBABA[0])

static const char *const MC_ALIBABA_CODING[] = {
    "qwen3.7-plus", "qwen3.6-plus", "qwen3.5-plus", "qwen3-max-2026-01-23", "qwen3-coder-plus", "qwen3-coder-next",
    "kimi-k2.5", "glm-5", "glm-4.7", "MiniMax-M2.5",
};
#define MC_ALIBABA_CODING_N (sizeof MC_ALIBABA_CODING / sizeof MC_ALIBABA_CODING[0])

static const char *const MC_ALIBABA_TOKEN[] = {
    "qwen3.8-max-0902", "qwen3.7-max", "qwen3.7-plus", "qwen3.6-plus", "qwen3.6-flash", "deepseek-v4-pro",
    "deepseek-v4-flash", "deepseek-v3.2", "kimi-k2.7-code", "kimi-k2.6", "kimi-k2.5", "glm-5.2",
    "glm-5.1", "glm-5",
};
#define MC_ALIBABA_TOKEN_N (sizeof MC_ALIBABA_TOKEN / sizeof MC_ALIBABA_TOKEN[0])

/* ---- capability sets and wire prefixes -------------------------------- */

static const char *const MC_SILENT_DEFAULT[] = {
    "nous", "openrouter",
};
#define MC_SILENT_DEFAULT_N (sizeof MC_SILENT_DEFAULT / sizeof MC_SILENT_DEFAULT[0])

static const char *const MC_AGGREGATOR[] = {
    "ai-gateway", "kilocode", "nous", "openrouter",
};
#define MC_AGGREGATOR_N (sizeof MC_AGGREGATOR / sizeof MC_AGGREGATOR[0])

static const char *const MC_BORROWED[] = {
    NULL                                  /* empty set, declared below */
};
#define MC_BORROWED_N 0

static const char *const MC_LIVE_FIRST[] = {
    "meta-ai", "opencode-go", "opencode-zen",
};
#define MC_LIVE_FIRST_N (sizeof MC_LIVE_FIRST / sizeof MC_LIVE_FIRST[0])

static const char *const MC_OPENAI_FAST_PREFIXES[] = {
    "gpt-", "o1", "o3", "o4",
};
#define MC_OPENAI_FAST_PREFIXES_N (sizeof MC_OPENAI_FAST_PREFIXES / sizeof MC_OPENAI_FAST_PREFIXES[0])

static const char *const MC_OPENAI_ULTRAFAST[] = {
    "gpt-6-astra", "gpt-6.1-sol", "gpt-6.2-astra", "gpt-6.2-sol",
};
#define MC_OPENAI_ULTRAFAST_N (sizeof MC_OPENAI_ULTRAFAST / sizeof MC_OPENAI_ULTRAFAST[0])

static const char *const MC_MODELS_DEV_PREFERRED[] = {
    "cohere", "fireworks", "gemini", "google", "groq", "huggingface",
    "kilocode", "mistral", "nvidia", "opencode-go", "opencode-zen", "perplexity",
    "togetherai", "xai", "xai-oauth", "zai",
};
#define MC_MODELS_DEV_PREFERRED_N (sizeof MC_MODELS_DEV_PREFERRED / sizeof MC_MODELS_DEV_PREFERRED[0])

static const char *const MC_AZURE_RESPONSES_PREFIXES[] = {
    "codex", "gpt-5", "o1", "o3", "o4",
};
#define MC_AZURE_RESPONSES_PREFIXES_N (sizeof MC_AZURE_RESPONSES_PREFIXES / sizeof MC_AZURE_RESPONSES_PREFIXES[0])

static const char *const MC_BUILTIN_SLUGS[] = {
    "ai-gateway", "alibaba", "anthropic", "arcee", "azure-foundry", "bedrock",
    "deepseek", "fireworks", "gemini", "gmi", "huggingface", "kilocode",
    "kimi-coding", "kimi-coding-cn", "lmstudio", "minimax", "minimax-cn", "minimax-oauth",
    "moa", "nous", "novita", "nvidia", "ollama-cloud", "openai-api",
    "openai-codex", "opencode-go", "opencode-zen", "openrouter", "qwen-oauth", "stepfun",
    "tencent-tokenhub", "tencent-tokenplan", "vertex", "xai", "xai-oauth", "xiaomi",
    "zai",
};
#define MC_BUILTIN_SLUGS_N (sizeof MC_BUILTIN_SLUGS / sizeof MC_BUILTIN_SLUGS[0])

/* ---- curated per-provider rows (static ones only) --------------------- */

static const char *const MC_ROW_SLUGS[] = {
    "moa", "nous", "openai", "openai-api", "gemini", "zai",
    "nvidia", "kimi-coding", "kimi-coding-cn", "stepfun", "moonshot", "minimax",
    "minimax-oauth", "minimax-cn", "anthropic", "deepseek", "xiaomi", "tencent-tokenhub",
    "tencent-tokenplan", "arcee", "gmi", "opencode-zen", "opencode-go", "kilocode",
    "alibaba", "alibaba-cn", "alibaba-coding-plan", "alibaba-coding-plan-cn", "alibaba-token-plan", "alibaba-token-plan-cn",
    "huggingface", "bedrock", "azure-foundry", "vertex", "novita", "ai-gateway",
};
#define MC_ROW_SLUGS_N (sizeof MC_ROW_SLUGS / sizeof MC_ROW_SLUGS[0])

static const char *const MC_ROW_00[] = {
    "default",
};
#define MC_ROW_00_N (sizeof MC_ROW_00 / sizeof MC_ROW_00[0])

static const char *const MC_ROW_01[] = {
    "anthropic/claude-fable-5.2", "anthropic/claude-fable-5.1", "anthropic/claude-fable-5",
    "anthropic/claude-opus-5.5", "anthropic/claude-opus-5", "anthropic/claude-opus-4.8",
    "anthropic/claude-sonnet-5.5", "anthropic/claude-sonnet-5", "anthropic/claude-haiku-5.5",
    "anthropic/claude-haiku-4.5", "openai/gpt-6-astra", "openai/gpt-6-astra-fast",
    "openai/gpt-6-astra-flex", "openai/gpt-6-astra-pro", "openai/gpt-6-astra-pro-fast",
    "openai/gpt-6-astra-pro-flex", "openai/gpt-6.1-sol", "openai/gpt-6.1-sol-pro",
    "openai/gpt-6.1-terra", "openai/gpt-6-sol", "openai/gpt-6-sol-pro",
    "openai/gpt-6-luna", "openai/gpt-6-luna-pro", "openai/gpt-5.5",
    "openai/gpt-5.5-pro", "openai/gpt-5.4-mini", "google/gemini-4-argon",
    "google/gemini-3.1-pro-preview", "google/gemini-3.8-flash", "google/gemini-3.7-flash",
    "x-ai/grok-4.7", "x-ai/grok-4.6", "deepseek/deepseek-v4-pro",
    "deepseek/deepseek-v4-pro-0813", "deepseek/deepseek-v4.1-flash", "deepseek/deepseek-v4.1-flash-1017",
    "deepseek/deepseek-v4-flash-0731", "qwen/qwen3.8-max-0902", "qwen/qwen3.8-flash",
    "moonshotai/kimi-k3", "minimax/minimax-m3", "z-ai/glm-5.3",
    "z-ai/glm-5.3-flash", "z-ai/glm-5.3-flashx", "z-ai/glm-5.2",
    "xiaomi/mimo-v2.6-pro", "xiaomi/mimo-v2.6-flash", "xiaomi/mimo-v2.6-pro-ultraspeed",
    "xiaomi/mimo-v2.5-pro", "tencent/hy4-preview", "tencent/hy3",
    "stepfun/step-3.7-flash", "nvidia/nemotron-3-super-120b-a12b", "meta/muse-spark-1.2",
    "meta/muse-spark-1.3", "sakana/fugu-ultra", "upstage/solar-pro4",
    "upstage/solar-mini4", "stealth/space-bunny-alpha",
};
#define MC_ROW_01_N (sizeof MC_ROW_01 / sizeof MC_ROW_01[0])

static const char *const MC_ROW_02[] = {
    "gpt-5.4", "gpt-5.4-mini", "gpt-5-mini",
    "gpt-5.3-codex", "gpt-5.2-codex", "gpt-4.1",
    "gpt-4o", "gpt-4o-mini",
};
#define MC_ROW_02_N (sizeof MC_ROW_02 / sizeof MC_ROW_02[0])

static const char *const MC_ROW_03[] = {
    "gpt-6.2-sol", "gpt-6.2-astra", "gpt-6.1-sol",
    "gpt-6.1-sol-pro", "gpt-6-sol", "gpt-6-sol-pro",
    "gpt-6-luna", "gpt-6-luna-pro", "gpt-5.6-sol",
    "gpt-5.6-sol-pro", "gpt-5.6-terra", "gpt-5.6-terra-pro",
    "gpt-5.6-luna", "gpt-5.6-luna-pro", "gpt-5.5",
    "gpt-5.5-pro", "gpt-5.4", "gpt-5.4-mini",
    "gpt-5.4-nano", "gpt-5-mini", "gpt-5.3-codex",
    "gpt-4.1", "gpt-4o", "gpt-4o-mini",
};
#define MC_ROW_03_N (sizeof MC_ROW_03 / sizeof MC_ROW_03[0])

static const char *const MC_ROW_04[] = {
    "gemini-3.8-flash", "gemini-4-hydrogen", "gemini-4-carbon",
    "gemini-4-argon", "gemini-3.7-flash", "gemini-3.1-pro-preview",
    "gemini-3-pro-preview", "gemini-3.6-flash", "gemini-3.1-flash-lite-preview",
};
#define MC_ROW_04_N (sizeof MC_ROW_04 / sizeof MC_ROW_04[0])

static const char *const MC_ROW_05[] = {
    "glm-5.3", "glm-5.3-flash", "glm-5.2",
    "glm-5.1", "glm-5", "glm-5v-turbo",
    "glm-5-turbo", "glm-4.7", "glm-4.5",
    "glm-4.5-flash",
};
#define MC_ROW_05_N (sizeof MC_ROW_05 / sizeof MC_ROW_05[0])

static const char *const MC_ROW_06[] = {
    "nvidia/nemotron-3.5-super-vl-120b-a12b", "nvidia/nemotron-3-ultra-550b-a55b", "nvidia/nemotron-3-super-120b-a12b",
    "nvidia/nemotron-3.5-lightning-30b-a3b", "nvidia/nemotron-3-nano-omni-30b-a3b-reasoning", "z-ai/glm-5.3",
    "z-ai/glm-5.2", "moonshotai/kimi-k2.6", "minimaxai/minimax-m3",
};
#define MC_ROW_06_N (sizeof MC_ROW_06 / sizeof MC_ROW_06[0])

static const char *const MC_ROW_07[] = {
    "kimi-k3", "kimi-k2.7-code", "kimi-k2.6",
    "kimi-k2.5", "kimi-for-coding", "kimi-for-coding-highspeed",
    "kimi-k2-thinking", "kimi-k2-thinking-turbo", "kimi-k2-turbo-preview",
    "kimi-k2-0905-preview",
};
#define MC_ROW_07_N (sizeof MC_ROW_07 / sizeof MC_ROW_07[0])

static const char *const MC_ROW_08[] = {
    "kimi-k3", "kimi-k2.7-code", "kimi-k2.7-code-highspeed",
    "kimi-k2.6", "kimi-k2.5", "kimi-k2-thinking",
    "kimi-k2-turbo-preview", "kimi-k2-0905-preview",
};
#define MC_ROW_08_N (sizeof MC_ROW_08 / sizeof MC_ROW_08[0])

static const char *const MC_ROW_09[] = {
    "step-3.7-flash", "step-3.5-flash", "step-3.5-flash-2603",
};
#define MC_ROW_09_N (sizeof MC_ROW_09 / sizeof MC_ROW_09[0])

static const char *const MC_ROW_10[] = {
    "kimi-k3", "kimi-k2.6", "kimi-k2.5",
    "kimi-k2-thinking", "kimi-k2-turbo-preview", "kimi-k2-0905-preview",
};
#define MC_ROW_10_N (sizeof MC_ROW_10 / sizeof MC_ROW_10[0])

static const char *const MC_ROW_11[] = {
    "MiniMax-M3", "MiniMax-M2.7", "MiniMax-M2.5",
    "MiniMax-M2.1", "MiniMax-M2",
};
#define MC_ROW_11_N (sizeof MC_ROW_11 / sizeof MC_ROW_11[0])

static const char *const MC_ROW_12[] = {
    "MiniMax-M3", "MiniMax-M2.7", "MiniMax-M2.7-highspeed",
};
#define MC_ROW_12_N (sizeof MC_ROW_12 / sizeof MC_ROW_12[0])

static const char *const MC_ROW_13[] = {
    "MiniMax-M3", "MiniMax-M2.7", "MiniMax-M2.5",
    "MiniMax-M2.1", "MiniMax-M2",
};
#define MC_ROW_13_N (sizeof MC_ROW_13 / sizeof MC_ROW_13[0])

static const char *const MC_ROW_14[] = {
    "claude-fable-5-2", "claude-fable-5.1", "claude-fable-5",
    "claude-opus-5-5", "claude-opus-5", "claude-sonnet-5",
    "claude-opus-4-8", "claude-opus-4-7", "claude-opus-4-6",
    "claude-sonnet-4-6", "claude-opus-4-5-20251101", "claude-sonnet-4-5-20250929",
    "claude-opus-4-20250514", "claude-sonnet-4-20250514", "claude-haiku-4-5-20251001",
};
#define MC_ROW_14_N (sizeof MC_ROW_14 / sizeof MC_ROW_14[0])

static const char *const MC_ROW_15[] = {
    "deepseek-flash", "deepseek-v4-pro",
};
#define MC_ROW_15_N (sizeof MC_ROW_15 / sizeof MC_ROW_15[0])

static const char *const MC_ROW_16[] = {
    "mimo-v2.6-pro", "mimo-v2.6-flash", "mimo-v2.6-pro-ultraspeed",
    "mimo-v2.5-pro", "mimo-v2.5", "mimo-v2-pro",
    "mimo-v2-omni", "mimo-v2-flash",
};
#define MC_ROW_16_N (sizeof MC_ROW_16 / sizeof MC_ROW_16[0])

static const char *const MC_ROW_17[] = {
    "hy4-preview", "hy3", "hy3-preview",
};
#define MC_ROW_17_N (sizeof MC_ROW_17 / sizeof MC_ROW_17[0])

static const char *const MC_ROW_18[] = {
    "hy4-preview", "hy3", "hy3-preview",
};
#define MC_ROW_18_N (sizeof MC_ROW_18 / sizeof MC_ROW_18[0])

static const char *const MC_ROW_19[] = {
    "trinity-large-thinking", "trinity-large-preview", "trinity-mini",
};
#define MC_ROW_19_N (sizeof MC_ROW_19 / sizeof MC_ROW_19[0])

static const char *const MC_ROW_20[] = {
    "zai-org/GLM-5.1-FP8", "deepseek-ai/DeepSeek-V3.2", "moonshotai/Kimi-K2.5",
    "google/gemini-3.1-flash-lite-preview", "anthropic/claude-sonnet-5", "anthropic/claude-sonnet-4.6",
    "openai/gpt-5.4",
};
#define MC_ROW_20_N (sizeof MC_ROW_20 / sizeof MC_ROW_20[0])

static const char *const MC_ROW_21[] = {
    "x-preview-f-free", "kimi-k3", "kimi-k2.5",
    "kimi-k2.6", "gpt-5.6-sol", "gpt-5.6-terra",
    "gpt-5.6-luna", "gpt-5.5", "gpt-5.5-pro",
    "gpt-5.4-pro", "gpt-5.4", "gpt-5.4-mini",
    "gpt-5.4-nano", "gpt-5.3-codex", "gpt-5.3-codex-spark",
    "gpt-5.2", "gpt-5.2-codex", "gpt-5.1",
    "gpt-5.1-codex", "gpt-5.1-codex-max", "gpt-5.1-codex-mini",
    "gpt-5", "gpt-5-codex", "gpt-5-nano",
    "claude-fable-5", "claude-opus-5", "claude-sonnet-5",
    "claude-opus-4-8", "claude-opus-4-7", "claude-opus-4-6",
    "claude-opus-4-5", "claude-sonnet-4-6", "claude-sonnet-4-5",
    "claude-sonnet-4", "claude-haiku-4-5", "gemini-3.8-flash",
    "gemini-3.7-flash", "gemini-3.6-flash", "gemini-3.5-flash",
    "gemini-3.5-flash-lite", "gemini-3.1-pro", "gemini-3-flash",
    "grok-4.6", "grok-4.5", "grok-build-0.1",
    "muse-spark-1.2", "minimax-m3", "minimax-m2.7",
    "minimax-m2.5", "glm-5.3", "glm-5.3-flash",
    "glm-5.2", "glm-5.1", "glm-5",
    "kimi-k2.7-code", "deepseek-v4-pro", "deepseek-v4-flash",
    "qwen3.6-plus", "qwen3.5-plus", "big-pickle",
    "mimo-v2.5-free", "nemotron-3-ultra-free", "nemotron-3.5-lightning-free",
    "muse-spark-1.2-contributor-free", "muse-spark-1.3-contributor-free",
};
#define MC_ROW_21_N (sizeof MC_ROW_21 / sizeof MC_ROW_21[0])

static const char *const MC_ROW_22[] = {
    "kimi-k3", "kimi-k2.7-code", "kimi-k2.6",
    "kimi-k2.5", "gpt-5.6-luna", "grok-4.5",
    "glm-5.3", "glm-5.3-flash", "glm-5.2",
    "glm-5.1", "glm-5", "mimo-v2.5-pro",
    "mimo-v2.5", "mimo-v2-pro", "mimo-v2-omni",
    "minimax-m3", "minimax-m2.7", "minimax-m2.5",
    "deepseek-v4-pro", "deepseek-v4-flash", "qwen3.8-max",
    "qwen3.7-max", "qwen3.7-plus", "qwen3.6-plus",
    "qwen3.5-plus", "hy3", "hy3-preview",
    "muse-spark-1.2-contributor", "muse-spark-1.3-contributor",
};
#define MC_ROW_22_N (sizeof MC_ROW_22 / sizeof MC_ROW_22[0])

static const char *const MC_ROW_23[] = {
    "anthropic/claude-opus-4.6", "anthropic/claude-sonnet-4.6", "openai/gpt-5.4",
    "google/gemini-3-pro-preview", "google/gemini-3-flash-preview",
};
#define MC_ROW_23_N (sizeof MC_ROW_23 / sizeof MC_ROW_23[0])

static const char *const MC_ROW_24[] = {
    "qwen3.8-max", "qwen3.7-max", "qwen3.7-plus",
    "qwen3.6-plus", "qwen3.6-flash", "kimi-k2.5",
    "qwen3.5-plus", "qwen3-coder-plus", "qwen3-coder-next",
    "glm-5.2", "glm-5", "glm-4.7",
    "deepseek-v4-pro", "deepseek-v4-flash-0731", "MiniMax-M2.5",
};
#define MC_ROW_24_N (sizeof MC_ROW_24 / sizeof MC_ROW_24[0])

static const char *const MC_ROW_25[] = {
    "qwen3.8-max", "qwen3.7-max", "qwen3.7-plus",
    "qwen3.6-plus", "qwen3.6-flash", "kimi-k2.5",
    "qwen3.5-plus", "qwen3-coder-plus", "qwen3-coder-next",
    "glm-5.2", "glm-5", "glm-4.7",
    "deepseek-v4-pro", "deepseek-v4-flash-0731", "MiniMax-M2.5",
};
#define MC_ROW_25_N (sizeof MC_ROW_25 / sizeof MC_ROW_25[0])

static const char *const MC_ROW_26[] = {
    "qwen3.7-plus", "qwen3.6-plus", "qwen3.5-plus",
    "qwen3-max-2026-01-23", "qwen3-coder-plus", "qwen3-coder-next",
    "kimi-k2.5", "glm-5", "glm-4.7",
    "MiniMax-M2.5",
};
#define MC_ROW_26_N (sizeof MC_ROW_26 / sizeof MC_ROW_26[0])

static const char *const MC_ROW_27[] = {
    "qwen3.7-plus", "qwen3.6-plus", "qwen3.5-plus",
    "qwen3-max-2026-01-23", "qwen3-coder-plus", "qwen3-coder-next",
    "kimi-k2.5", "glm-5", "glm-4.7",
    "MiniMax-M2.5",
};
#define MC_ROW_27_N (sizeof MC_ROW_27 / sizeof MC_ROW_27[0])

static const char *const MC_ROW_28[] = {
    "qwen3.8-max-0902", "qwen3.7-max", "qwen3.7-plus",
    "qwen3.6-plus", "qwen3.6-flash", "deepseek-v4-pro",
    "deepseek-v4-flash", "deepseek-v3.2", "kimi-k2.7-code",
    "kimi-k2.6", "kimi-k2.5", "glm-5.2",
    "glm-5.1", "glm-5",
};
#define MC_ROW_28_N (sizeof MC_ROW_28 / sizeof MC_ROW_28[0])

static const char *const MC_ROW_29[] = {
    "qwen3.8-max-0902", "qwen3.7-max", "qwen3.7-plus",
    "qwen3.6-plus", "qwen3.6-flash", "deepseek-v4-pro",
    "deepseek-v4-flash", "deepseek-v3.2", "kimi-k2.7-code",
    "kimi-k2.6", "kimi-k2.5", "glm-5.2",
    "glm-5.1", "glm-5",
};
#define MC_ROW_29_N (sizeof MC_ROW_29 / sizeof MC_ROW_29[0])

static const char *const MC_ROW_30[] = {
    "moonshotai/Kimi-K2.5", "Qwen/Qwen3.5-397B-A17B", "Qwen/Qwen3.5-35B-A3B",
    "deepseek-ai/DeepSeek-V3.2", "MiniMaxAI/MiniMax-M2.5", "zai-org/GLM-5",
    "XiaomiMiMo/MiMo-V2-Flash", "moonshotai/Kimi-K2-Thinking", "moonshotai/Kimi-K2.6",
};
#define MC_ROW_30_N (sizeof MC_ROW_30 / sizeof MC_ROW_30[0])

static const char *const MC_ROW_31[] = {
    "us.anthropic.claude-sonnet-5", "us.anthropic.claude-opus-5-5", "us.anthropic.claude-sonnet-4-6",
    "us.anthropic.claude-opus-4-6-v1", "us.anthropic.claude-haiku-4-5-20251001-v1:0", "us.anthropic.claude-sonnet-4-5-20250929-v1:0",
    "openai.gpt-6-astra", "openai.gpt-6.1-sol", "openai.gpt-6.1-terra",
    "openai.gpt-6-sol", "openai.gpt-6-luna", "openai.gpt-5.5",
    "openai.gpt-5.6-sol", "openai.gpt-5.6-terra", "openai.gpt-5.6-luna",
    "openai.gpt-5.4", "us.amazon.nova-pro-v1:0", "us.amazon.nova-lite-v1:0",
    "us.amazon.nova-micro-v1:0", "deepseek.v3.2", "us.meta.llama4-maverick-17b-instruct-v1:0",
    "us.meta.llama4-scout-17b-instruct-v1:0",
};
#define MC_ROW_31_N (sizeof MC_ROW_31 / sizeof MC_ROW_31[0])

static const char *const MC_ROW_32[] = {
    NULL                                  /* azure-foundry: empty curated row */
};
#define MC_ROW_32_N 0

static const char *const MC_ROW_33[] = {
    "google/gemini-3.8-flash", "google/gemini-4-argon", "google/gemini-3.7-flash",
    "google/gemini-3.1-pro-preview", "google/gemini-3-pro-preview", "google/gemini-3.6-flash",
    "google/gemini-3.5-flash", "google/gemini-3.5-flash-lite", "google/gemini-3-flash-preview",
    "google/gemini-3.1-flash-lite-preview", "google/gemini-3.1-flash-lite",
};
#define MC_ROW_33_N (sizeof MC_ROW_33 / sizeof MC_ROW_33[0])

static const char *const MC_ROW_34[] = {
    "moonshotai/kimi-k2.5", "minimax/minimax-m2.7", "zai-org/glm-5",
    "deepseek/deepseek-v3-0324", "deepseek/deepseek-r1-0528", "qwen/qwen3-235b-a22b-fp8",
};
#define MC_ROW_34_N (sizeof MC_ROW_34 / sizeof MC_ROW_34[0])

static const char *const MC_ROW_35[] = {
    "moonshotai/kimi-k2.6", "alibaba/qwen3.6-plus", "zai/glm-5.1",
    "minimax/minimax-m2.7", "anthropic/claude-sonnet-4.6", "anthropic/claude-opus-4.7",
    "anthropic/claude-opus-4.6", "anthropic/claude-haiku-4.5", "openai/gpt-5.4",
    "openai/gpt-5.4-mini", "openai/gpt-5.3-codex", "google/gemini-3.1-pro-preview",
    "google/gemini-3-flash", "google/gemini-3.1-flash-lite-preview", "xai/grok-4.20-reasoning",
};
#define MC_ROW_35_N (sizeof MC_ROW_35 / sizeof MC_ROW_35[0])

static const char *const *const MC_ROW_IDS[] = {
    MC_ROW_00,
    MC_ROW_01,
    MC_ROW_02,
    MC_ROW_03,
    MC_ROW_04,
    MC_ROW_05,
    MC_ROW_06,
    MC_ROW_07,
    MC_ROW_08,
    MC_ROW_09,
    MC_ROW_10,
    MC_ROW_11,
    MC_ROW_12,
    MC_ROW_13,
    MC_ROW_14,
    MC_ROW_15,
    MC_ROW_16,
    MC_ROW_17,
    MC_ROW_18,
    MC_ROW_19,
    MC_ROW_20,
    MC_ROW_21,
    MC_ROW_22,
    MC_ROW_23,
    MC_ROW_24,
    MC_ROW_25,
    MC_ROW_26,
    MC_ROW_27,
    MC_ROW_28,
    MC_ROW_29,
    MC_ROW_30,
    MC_ROW_31,
    MC_ROW_32,
    MC_ROW_33,
    MC_ROW_34,
    MC_ROW_35,
};
static const size_t MC_ROW_LEN[] = {
    MC_ROW_00_N, MC_ROW_01_N, MC_ROW_02_N, MC_ROW_03_N, MC_ROW_04_N, MC_ROW_05_N, MC_ROW_06_N, MC_ROW_07_N,
    MC_ROW_08_N, MC_ROW_09_N, MC_ROW_10_N, MC_ROW_11_N, MC_ROW_12_N, MC_ROW_13_N, MC_ROW_14_N, MC_ROW_15_N,
    MC_ROW_16_N, MC_ROW_17_N, MC_ROW_18_N, MC_ROW_19_N, MC_ROW_20_N, MC_ROW_21_N, MC_ROW_22_N, MC_ROW_23_N,
    MC_ROW_24_N, MC_ROW_25_N, MC_ROW_26_N, MC_ROW_27_N, MC_ROW_28_N, MC_ROW_29_N, MC_ROW_30_N, MC_ROW_31_N,
    MC_ROW_32_N, MC_ROW_33_N, MC_ROW_34_N, MC_ROW_35_N
};
static const char *const MC_DYNAMIC_ROWS[] = {
    "xai", "xai-oauth", "openai-codex",
};
#define MC_ROW_COUNT (sizeof MC_ROW_SLUGS / sizeof MC_ROW_SLUGS[0])

/* ---- canonical providers and labels ---------------------------------- */

static const char *const MC_PROV[] = {
    "nous", "Nous Portal", "Nous Portal (Everything your agent needs, 300+ models with bundled tool use)",
    "fireworks", "Fireworks AI", "Fireworks AI (OpenAI-compatible direct model API)",
    "openrouter", "OpenRouter", "OpenRouter (Pay-per-use API aggregator)",
    "moa", "Mixture of Agents", "Mixture of Agents (named presets; aggregator acts after reference models)",
    "novita", "NovitaAI", "NovitaAI (Cloud: Model API, Agent Sandbox, GPU Cloud)",
    "lmstudio", "LM Studio", "LM Studio (Local desktop app with built-in model server)",
    "anthropic", "Anthropic", "Anthropic (Claude models via API key or Claude Code)",
    "openai-codex", "ChatGPT or Codex Subscription", "ChatGPT or Codex Subscription (Sign in with your ChatGPT account, uses Codex models)",
    "openai-api", "OpenAI API", "OpenAI API (api.openai.com, API key)",
    "alibaba", "Qwen Cloud", "Qwen Cloud / DashScope (Qwen + multi-provider)",
    "xai-oauth", "xAI Grok OAuth (SuperGrok / Premium+)", "xAI Grok OAuth (SuperGrok / Premium+ subscription)",
    "xiaomi", "Xiaomi MiMo", "Xiaomi MiMo (MiMo-V2.5 and V2 models: pro, omni, flash)",
    "tencent-tokenhub", "Tencent TokenHub", "Tencent TokenHub (Hy4 preview via tokenhub.tencentmaas.com)",
    "tencent-tokenplan", "Tencent TokenPlan", "Tencent TokenPlan (Hy4 preview via api.lkeap.cloud.tencent.com, Anthropic Messages)",
    "nvidia", "NVIDIA NIM", "NVIDIA NIM (Nemotron models via build.nvidia.com or local NIM)",
    "huggingface", "Hugging Face", "Hugging Face Inference Providers",
    "gemini", "Google AI Studio", "Google AI Studio (Native Gemini API)",
    "vertex", "Google Vertex AI", "Google Vertex AI (Gemini via GCP; OAuth2 service account or ADC, GCP billing/quotas)",
    "deepseek", "DeepSeek", "DeepSeek (V3, R1, coder, direct API)",
    "xai", "xAI", "xAI Grok (Direct API)",
    "zai", "Z.AI / GLM", "Z.AI / GLM (Zhipu direct API)",
    "kimi-coding", "Kimi / Kimi Coding Plan", "Kimi Coding Plan (api.kimi.com & Moonshot API)",
    "kimi-coding-cn", "Kimi / Moonshot (China)", "Kimi / Moonshot China (Domestic direct API)",
    "stepfun", "StepFun Step Plan", "StepFun Step Plan (Agent / coding models via Step Plan API)",
    "minimax", "MiniMax", "MiniMax (Global direct API)",
    "minimax-oauth", "MiniMax (OAuth)", "MiniMax via OAuth browser login (Coding Plan, minimax.io)",
    "minimax-cn", "MiniMax (China)", "MiniMax China (Domestic direct API)",
    "ollama-cloud", "Ollama Cloud", "Ollama Cloud (Cloud-hosted open models, ollama.com)",
    "arcee", "Arcee AI", "Arcee AI (Trinity models, direct API)",
    "gmi", "GMI Cloud", "GMI Cloud (Multi-model direct API)",
    "kilocode", "Kilo Code", "Kilo Code (Kilo Gateway API)",
    "opencode-zen", "OpenCode Zen", "OpenCode Zen (Curated models, pay-as-you-go)",
    "opencode-go", "OpenCode Go", "OpenCode Go (Open models subscription)",
    "bedrock", "AWS Bedrock", "AWS Bedrock (Claude, Nova, Llama, DeepSeek; IAM or API key)",
    "azure-foundry", "Azure Foundry", "Azure Foundry (OpenAI-style or Anthropic-style endpoint, your Azure AI deployment)",
    "ai-gateway", "Vercel AI Gateway", "Vercel AI Gateway (Multi-model aggregator)",
    "qwen-oauth", "Qwen OAuth (Portal)", "Qwen OAuth (Reuses local Qwen CLI login)",
};
#define MC_PROV_COUNT 37

static const char *const MC_LABELS[] = {
    "ai-gateway", "Vercel AI Gateway",
    "alibaba", "Qwen Cloud",
    "anthropic", "Anthropic",
    "arcee", "Arcee AI",
    "azure-foundry", "Azure Foundry",
    "bedrock", "AWS Bedrock",
    "custom", "Custom endpoint",
    "deepseek", "DeepSeek",
    "fireworks", "Fireworks AI",
    "gemini", "Google AI Studio",
    "gmi", "GMI Cloud",
    "huggingface", "Hugging Face",
    "kilocode", "Kilo Code",
    "kimi-coding", "Kimi / Kimi Coding Plan",
    "kimi-coding-cn", "Kimi / Moonshot (China)",
    "lmstudio", "LM Studio",
    "minimax", "MiniMax",
    "minimax-cn", "MiniMax (China)",
    "minimax-oauth", "MiniMax (OAuth)",
    "moa", "Mixture of Agents",
    "nous", "Nous Portal",
    "novita", "NovitaAI",
    "nvidia", "NVIDIA NIM",
    "ollama-cloud", "Ollama Cloud",
    "openai-api", "OpenAI API",
    "openai-codex", "ChatGPT or Codex Subscription",
    "opencode-go", "OpenCode Go",
    "opencode-zen", "OpenCode Zen",
    "openrouter", "OpenRouter",
    "qwen-oauth", "Qwen OAuth (Portal)",
    "stepfun", "StepFun Step Plan",
    "tencent-tokenhub", "Tencent TokenHub",
    "tencent-tokenplan", "Tencent TokenPlan",
    "vertex", "Google Vertex AI",
    "xai", "xAI",
    "xai-oauth", "xAI Grok OAuth (SuperGrok / Premium+)",
    "xiaomi", "Xiaomi MiMo",
    "zai", "Z.AI / GLM",
};
#define MC_LABEL_COUNT 38

/* ---- display groups and the slug -> group index ----------------------- */

static const char *const MC_GMEM_KIMI[] = {
    "kimi-coding", "kimi-coding-cn",
};
#define MC_GMEM_KIMI_N (sizeof MC_GMEM_KIMI / sizeof MC_GMEM_KIMI[0])

static const char *const MC_GMEM_MINIMAX[] = {
    "minimax", "minimax-oauth", "minimax-cn",
};
#define MC_GMEM_MINIMAX_N (sizeof MC_GMEM_MINIMAX / sizeof MC_GMEM_MINIMAX[0])

static const char *const MC_GMEM_XAI[] = {
    "xai", "xai-oauth",
};
#define MC_GMEM_XAI_N (sizeof MC_GMEM_XAI / sizeof MC_GMEM_XAI[0])

static const char *const MC_GMEM_GOOGLE[] = {
    "gemini",
};
#define MC_GMEM_GOOGLE_N (sizeof MC_GMEM_GOOGLE / sizeof MC_GMEM_GOOGLE[0])

static const char *const MC_GMEM_OPENAI[] = {
    "openai-codex", "openai-api",
};
#define MC_GMEM_OPENAI_N (sizeof MC_GMEM_OPENAI / sizeof MC_GMEM_OPENAI[0])

static const char *const MC_GMEM_QWEN[] = {
    "alibaba", "alibaba-cn", "alibaba-coding-plan", "alibaba-coding-plan-cn",
    "alibaba-token-plan", "alibaba-token-plan-cn", "qwen-oauth",
};
#define MC_GMEM_QWEN_N (sizeof MC_GMEM_QWEN / sizeof MC_GMEM_QWEN[0])

static const char *const MC_GMEM_OPENCODE[] = {
    "opencode-zen", "opencode-go",
};
#define MC_GMEM_OPENCODE_N (sizeof MC_GMEM_OPENCODE / sizeof MC_GMEM_OPENCODE[0])

static const char *const MC_GMEM_TENCENT[] = {
    "tencent-tokenhub", "tencent-tokenplan",
};
#define MC_GMEM_TENCENT_N (sizeof MC_GMEM_TENCENT / sizeof MC_GMEM_TENCENT[0])

static const struct { const char *gid, *label, *desc;
                      const char *const *members; size_t n; } MC_GROUPS[] = {
    { "kimi", "Kimi / Moonshot", "Coding Plan, Moonshot global & China endpoints", MC_GMEM_KIMI, MC_GMEM_KIMI_N },
    { "minimax", "MiniMax", "Global, OAuth Coding Plan & China endpoints", MC_GMEM_MINIMAX, MC_GMEM_MINIMAX_N },
    { "xai", "xAI Grok", "Direct API or SuperGrok / Premium+ OAuth", MC_GMEM_XAI, MC_GMEM_XAI_N },
    { "google", "Google Gemini", "Google AI Studio (API key)", MC_GMEM_GOOGLE, MC_GMEM_GOOGLE_N },
    { "openai", "OpenAI", "ChatGPT/Codex subscription or direct OpenAI API", MC_GMEM_OPENAI, MC_GMEM_OPENAI_N },
    { "qwen", "Qwen", "Qwen Cloud / DashScope, Coding Plan, Token Plan & Qwen CLI OAuth", MC_GMEM_QWEN, MC_GMEM_QWEN_N },
    { "opencode", "OpenCode", "Zen pay-as-you-go or Go subscription", MC_GMEM_OPENCODE, MC_GMEM_OPENCODE_N },
    { "tencent", "Tencent Hy", "Hy4 / Hy3 via TokenHub & TokenPlan", MC_GMEM_TENCENT, MC_GMEM_TENCENT_N },
};
#define MC_GROUP_COUNT 8

static const char *const MC_SLUG2GROUP[] = {
    "alibaba", "qwen",
    "alibaba-cn", "qwen",
    "alibaba-coding-plan", "qwen",
    "alibaba-coding-plan-cn", "qwen",
    "alibaba-token-plan", "qwen",
    "alibaba-token-plan-cn", "qwen",
    "gemini", "google",
    "kimi-coding", "kimi",
    "kimi-coding-cn", "kimi",
    "minimax", "minimax",
    "minimax-cn", "minimax",
    "minimax-oauth", "minimax",
    "openai-api", "openai",
    "openai-codex", "openai",
    "opencode-go", "opencode",
    "opencode-zen", "opencode",
    "qwen-oauth", "qwen",
    "tencent-tokenhub", "tencent",
    "tencent-tokenplan", "tencent",
    "xai", "xai",
    "xai-oauth", "xai",
};
#define MC_SLUG2GROUP_COUNT 21

static const char *const MC_ALIASES[] = {
    "glm", "zai",
    "z-ai", "zai",
    "z.ai", "zai",
    "zhipu", "zai",
    "google", "gemini",
    "google-gemini", "gemini",
    "google-ai-studio", "gemini",
    "google-vertex", "vertex",
    "vertex-ai", "vertex",
    "gcp-vertex", "vertex",
    "vertexai", "vertex",
    "kimi", "kimi-coding",
    "moonshot", "kimi-coding",
    "kimi-cn", "kimi-coding-cn",
    "moonshot-cn", "kimi-coding-cn",
    "step", "stepfun",
    "stepfun-coding-plan", "stepfun",
    "arcee-ai", "arcee",
    "arceeai", "arcee",
    "gmi-cloud", "gmi",
    "gmicloud", "gmi",
    "fireworks-ai", "fireworks",
    "fw", "fireworks",
    "actual-computer", "actual",
    "actualcomputer", "actual",
    "aci", "actual",
    "nebius", "nebius-token-factory",
    "nebius-tokenfactory", "nebius-token-factory",
    "nebius-tf", "nebius-token-factory",
    "token-factory", "nebius-token-factory",
    "tokenfactory", "nebius-token-factory",
    "minimax-china", "minimax-cn",
    "minimax_cn", "minimax-cn",
    "minimax-portal", "minimax-oauth",
    "minimax-global", "minimax-oauth",
    "minimax_oauth", "minimax-oauth",
    "claude", "anthropic",
    "claude-code", "anthropic",
    "deep-seek", "deepseek",
    "opencode", "opencode-zen",
    "zen", "opencode-zen",
    "go", "opencode-go",
    "opencode-go-sub", "opencode-go",
    "aigateway", "ai-gateway",
    "vercel", "ai-gateway",
    "vercel-ai-gateway", "ai-gateway",
    "kilo", "kilocode",
    "kilo-code", "kilocode",
    "kilo-gateway", "kilocode",
    "dashscope", "alibaba",
    "aliyun", "alibaba",
    "qwen", "alibaba",
    "alibaba-cloud", "alibaba",
    "qwen-portal", "qwen-oauth",
    "hf", "huggingface",
    "hugging-face", "huggingface",
    "huggingface-hub", "huggingface",
    "novita-ai", "novita",
    "novitaai", "novita",
    "mimo", "xiaomi",
    "xiaomi-mimo", "xiaomi",
    "tencent", "tencent-tokenhub",
    "tokenhub", "tencent-tokenhub",
    "tencent-cloud", "tencent-tokenhub",
    "tencentmaas", "tencent-tokenhub",
    "tokenplan", "tencent-tokenplan",
    "tencent-lkeap", "tencent-tokenplan",
    "aws", "bedrock",
    "aws-bedrock", "bedrock",
    "amazon-bedrock", "bedrock",
    "amazon", "bedrock",
    "grok", "xai",
    "grok-oauth", "xai-oauth",
    "xai-oauth", "xai-oauth",
    "x-ai-oauth", "xai-oauth",
    "xai-grok-oauth", "xai-oauth",
    "x-ai", "xai",
    "x.ai", "xai",
    "nim", "nvidia",
    "nvidia-nim", "nvidia",
    "build-nvidia", "nvidia",
    "nemotron", "nvidia",
    "lmstudio", "lmstudio",
    "lm-studio", "lmstudio",
    "lm_studio", "lmstudio",
    "chatgpt", "openai-codex",
    "chatgpt-codex", "openai-codex",
    "ollama", "custom",
    "ollama_cloud", "ollama-cloud",
    "local", "custom",
    "vllm", "custom",
};
#define MC_ALIAS_COUNT 91

static const char *const MC_RETIRED_DEEPSEEK[] = {
    "deepseek-chat", "deepseek-reasoner",
};
#define MC_RETIRED_DEEPSEEK_N (sizeof MC_RETIRED_DEEPSEEK / sizeof MC_RETIRED_DEEPSEEK[0])

static const char *const MC_RETIRED_SLUGS[] = {
    "deepseek",
};
static const char *const *const MC_RETIRED_IDS[] = {
    MC_RETIRED_DEEPSEEK,
};
static const size_t MC_RETIRED_LEN[] = {
    MC_RETIRED_DEEPSEEK_N,
};
#define MC_RETIRED_COUNT 1

#define MC_XAI_TOP_MODEL "grok-4.6"
#define MC_PREFERRED_SILENT_DEFAULT "z-ai/glm-5.3"


/* ---- shared helpers ---------------------------------------------------- */

static int mc_is_number(const json_t *v, double *d) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *d = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *d = v->bool_val ? 1.0 : 0.0; return 1; }
    return 0;
}

/* Python "==" over the shapes a model id can hold; a missing key equals null. */
static int mc_equal(const json_t *a, const json_t *b) {
    double x = 0.0, y = 0.0;
    size_t i, n;
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;
    if (mc_is_number(a, &x) && mc_is_number(b, &y)) return x == y;
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    if (a->type == JSON_BOOL && b->type == JSON_BOOL) return a->bool_val == b->bool_val;
    if (a->type == JSON_ARRAY && b->type == JSON_ARRAY) {
        n = json_len(a);
        if (n != json_len(b)) return 0;
        for (i = 0; i < n; i++)
            if (!mc_equal(json_get(a, i), json_get(b, i))) return 0;
        return 1;
    }
    if (a->type == JSON_OBJECT && b->type == JSON_OBJECT) {
        n = json_object_size(a);
        if (n != json_object_size(b)) return 0;
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(a, i);
            if (!k || !mc_equal(json_object_get_at(a, i), json_obj_get(b, k))) return 0;
        }
        return 1;
    }
    return 0;
}

#define MC_SLUG_CAP 160

static int mc_space(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* str(slug or "").strip().lower() - the normalisation every provider row uses. */
static void mc_norm(const json_t *v, char *buf) {
    const char *src;
    char tmp[64];
    size_t len, i;
    buf[0] = '\0';
    if (!v || v->type == JSON_NULL) return;
    if (v->type == JSON_STRING) {
        src = v->str_val ? v->str_val : "";
    } else if (v->type == JSON_BOOL) {
        src = v->bool_val ? "True" : "False";
    } else if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == (double)(long long)d && d > -1e15 && d < 1e15)
            snprintf(tmp, sizeof tmp, "%lld", (long long)d);
        else
            snprintf(tmp, sizeof tmp, "%g", d);
        src = tmp;
    } else {
        return;                     /* repr() of a container is not a slug */
    }
    while (mc_space((unsigned char)*src)) src++;
    len = strlen(src);
    while (len && mc_space((unsigned char)src[len - 1])) len--;
    if (len > MC_SLUG_CAP - 1) len = MC_SLUG_CAP - 1;
    for (i = 0; i < len; i++) {
        unsigned char c = (unsigned char)src[i];
        buf[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : (char)c;
    }
    buf[len] = '\0';
}

static int mc_in(const char *const *v, size_t n, const char *s) {
    size_t i;
    for (i = 0; i < n; i++)
        if (v[i] && !strcmp(v[i], s)) return 1;
    return 0;
}

/* The string a JSON value denotes, or "" for null/containers: table keys are
 * always plain strings upstream, so non-strings simply miss. */
static const char *mc_key(const json_t *v) {
    if (!v || v->type != JSON_STRING) return "";
    return v->str_val ? v->str_val : "";
}

static json_t *mc_str_array(const char *const *v, size_t n) {
    json_t *a = json_array();
    size_t i;
    for (i = 0; i < n; i++) json_append(a, json_string(v[i] ? v[i] : ""));
    return a;
}

/* Copy a list, optionally skipping every element equal to `skip`. */
static json_t *mc_copy_list(const json_t *ids, const json_t *skip) {
    json_t *a = json_array();
    size_t i, n;
    if (!ids || ids->type != JSON_ARRAY) return a;
    n = json_len(ids);
    for (i = 0; i < n; i++) {
        const json_t *e = json_get(ids, i);
        if (skip && mc_equal(e, skip)) continue;
        json_append(a, e ? json_copy(e) : json_null());
    }
    return a;
}

static int mc_list_has(const json_t *ids, const char *s) {
    size_t i, n;
    if (!ids || ids->type != JSON_ARRAY) return 0;
    n = json_len(ids);
    for (i = 0; i < n; i++) {
        const json_t *e = json_get(ids, i);
        if (e && e->type == JSON_STRING && !strcmp(e->str_val ? e->str_val : "", s)) return 1;
    }
    return 0;
}

/* list.insert(pos, s): the node at pos shifts right, the new one takes its
 * place - nothing is freed, the shift preserves every element once. */
static void mc_list_insert(json_t *ids, size_t pos, const char *s) {
    size_t n, i;
    json_t **ni;
    if (!ids || ids->type != JSON_ARRAY) return;
    n = ids->c.count;
    if (pos > n) pos = n;
    ni = (json_t **)realloc(ids->c.items, (n + 1) * sizeof(json_t *));
    if (!ni) return;
    ids->c.items = ni;
    for (i = n; i > pos; i--) ids->c.items[i] = ids->c.items[i - 1];
    ids->c.items[pos] = json_string(s);
    ids->c.count = n + 1;
}

/* ---- constants --------------------------------------------------------- */

json_t *w30_modelscata_4ee086_constants(void) {
    json_t *o = json_object(), *p, *a, *g;
    size_t i, j;

    p = json_object();
    for (i = 0; i < MC_GROUP_COUNT; i++) {
        g = json_array();
        json_append(g, json_string(MC_GROUPS[i].label));
        json_append(g, json_string(MC_GROUPS[i].desc));
        json_append(g, mc_str_array(MC_GROUPS[i].members, MC_GROUPS[i].n));
        json_set(p, MC_GROUPS[i].gid, g);
    }
    json_set(o, "provider_groups", p);

    p = json_object();
    for (i = 0; i < MC_GROUP_COUNT; i++) {
        for (j = 0; j < MC_GROUPS[i].n; j++)
            json_set(p, MC_GROUPS[i].members[j], json_string(MC_GROUPS[i].gid));
    }
    json_set(o, "slug_to_group", p);

    p = json_object();
    for (i = 0; i < MC_ROW_COUNT; i++)
        json_set(p, MC_ROW_SLUGS[i], mc_str_array(MC_ROW_IDS[i], MC_ROW_LEN[i]));
    json_set(o, "provider_models", p);

    a = json_array();
    for (i = 0; i < MC_PROV_COUNT; i++) {
        json_t *row = json_array();
        json_append(row, json_string(MC_PROV[i * 3]));
        json_append(row, json_string(MC_PROV[i * 3 + 1]));
        json_append(row, json_string(MC_PROV[i * 3 + 2]));
        json_append(a, row);
    }
    json_set(o, "canonical_providers", a);

    p = json_object();
    for (i = 0; i < MC_LABEL_COUNT; i++)
        json_set(p, MC_LABELS[i * 2], json_string(MC_LABELS[i * 2 + 1]));
    json_set(o, "provider_labels", p);

    p = json_object();
    for (i = 0; i < MC_ALIAS_COUNT; i++)
        json_set(p, MC_ALIASES[i * 2], json_string(MC_ALIASES[i * 2 + 1]));
    json_set(o, "provider_aliases", p);

    p = json_object();
    for (i = 0; i < MC_RETIRED_COUNT; i++)
        json_set(p, MC_RETIRED_SLUGS[i], mc_str_array(MC_RETIRED_IDS[i], MC_RETIRED_LEN[i]));
    json_set(o, "provider_retired_aliases", p);

    a = json_array();
    for (i = 0; i < MC_OPENROUTER_N; i++) {
        json_t *row = json_array();
        json_append(row, json_string(MC_OPENROUTER[i]));
        json_append(row, json_string(MC_OPENROUTER_DESC[i]));
        json_append(a, row);
    }
    json_set(o, "openrouter_models", a);

    a = json_array();
    for (i = 0; i < MC_VERCEL_N; i++) {
        json_t *row = json_array();
        json_append(row, json_string(MC_VERCEL[i]));
        json_append(row, json_string(MC_VERCEL_DESC[i]));
        json_append(a, row);
    }
    json_set(o, "vercel_ai_gateway_models", a);

    json_set(o, "openrouter_only", mc_str_array(MC_OPENROUTER_ONLY, MC_OPENROUTER_ONLY_N));
    json_set(o, "xai_static_fallback", mc_str_array(MC_XAI_STATIC, MC_XAI_STATIC_N));
    json_set(o, "xai_curated_extras", mc_str_array(MC_XAI_EXTRAS, MC_XAI_EXTRAS_N));
    json_set(o, "xai_top_model", json_string(MC_XAI_TOP_MODEL));
    json_set(o, "preferred_silent_default_model", json_string(MC_PREFERRED_SILENT_DEFAULT));
    json_set(o, "silent_default_providers", mc_str_array(MC_SILENT_DEFAULT, MC_SILENT_DEFAULT_N));
    json_set(o, "aggregator_providers", mc_str_array(MC_AGGREGATOR, MC_AGGREGATOR_N));
    json_set(o, "borrowed_model_providers", mc_str_array(MC_BORROWED, MC_BORROWED_N));
    json_set(o, "live_first_picker_providers", mc_str_array(MC_LIVE_FIRST, MC_LIVE_FIRST_N));
    json_set(o, "openai_fast_mode_prefixes", mc_str_array(MC_OPENAI_FAST_PREFIXES, MC_OPENAI_FAST_PREFIXES_N));
    json_set(o, "openai_ultrafast_models", mc_str_array(MC_OPENAI_ULTRAFAST, MC_OPENAI_ULTRAFAST_N));
    json_set(o, "models_dev_preferred", mc_str_array(MC_MODELS_DEV_PREFERRED, MC_MODELS_DEV_PREFERRED_N));
    json_set(o, "azure_foundry_responses_prefixes",
             mc_str_array(MC_AZURE_RESPONSES_PREFIXES, MC_AZURE_RESPONSES_PREFIXES_N));
    json_set(o, "builtin_slugs", mc_str_array(MC_BUILTIN_SLUGS, MC_BUILTIN_SLUGS_N));
    json_set(o, "dynamic_model_rows", mc_str_array(MC_DYNAMIC_ROWS, 3));
    json_set(o, "openai_chat_models", mc_str_array(MC_OPENAI_CHAT, MC_OPENAI_CHAT_N));
    json_set(o, "minimax_models", mc_str_array(MC_MINIMAX, MC_MINIMAX_N));
    json_set(o, "tencent_models", mc_str_array(MC_TENCENT, MC_TENCENT_N));
    json_set(o, "alibaba_models", mc_str_array(MC_ALIBABA, MC_ALIBABA_N));
    json_set(o, "alibaba_coding_plan_models", mc_str_array(MC_ALIBABA_CODING, MC_ALIBABA_CODING_N));
    json_set(o, "alibaba_token_plan_models", mc_str_array(MC_ALIBABA_TOKEN, MC_ALIBABA_TOKEN_N));
    return o;
}

/* ---- xAI catalogue finaliser ------------------------------------------- */

json_t *w30_modelscata_4ee086_xai_promote_top(const json_t *ids) {
    json_t *a, *top;
    if (!ids || ids->type != JSON_ARRAY) return mc_copy_list(NULL, NULL);
    top = json_string(MC_XAI_TOP_MODEL);
    if (!mc_list_has(ids, MC_XAI_TOP_MODEL)) {
        json_free(top);
        return mc_copy_list(ids, NULL);
    }
    a = json_array();
    json_append(a, top);
    {
        json_t *rest = mc_copy_list(ids, top);
        size_t i, n = json_len(rest);
        for (i = 0; i < n; i++) {
            json_t *e = json_get(rest, i);
            json_append(a, e ? json_copy(e) : json_null());
        }
        json_free(rest);
    }
    return a;
}

json_t *w30_modelscata_4ee086_xai_merge_curated_extras(const json_t *ids) {
    json_t *a;
    size_t i;
    if (!ids || ids->type != JSON_ARRAY) return mc_copy_list(NULL, NULL);
    a = mc_copy_list(ids, NULL);
    for (i = 0; i < MC_XAI_EXTRAS_N; i++) {
        const char *extra = MC_XAI_EXTRAS[i];
        if (mc_list_has(a, extra)) continue;
        {
            const json_t *head = json_get(a, 0);
            int pin_first = head && head->type == JSON_STRING && head->str_val
                            && !strcmp(head->str_val, MC_XAI_TOP_MODEL);
            mc_list_insert(a, pin_first ? 1 : json_len(a), extra);
        }
    }
    return a;
}

json_t *w30_modelscata_4ee086_xai_finalize_catalog(const json_t *ids) {
    json_t *merged = w30_modelscata_4ee086_xai_merge_curated_extras(ids);
    json_t *out = w30_modelscata_4ee086_xai_promote_top(merged);
    json_free(merged);
    return out;
}

/* ---- provider grouping (display only) --------------------------------- */

json_t *w30_modelscata_4ee086_provider_group_for_slug(const json_t *slug) {
    char buf[MC_SLUG_CAP];
    size_t i;
    mc_norm(slug, buf);
    if (!buf[0]) return json_string("");
    for (i = 0; i < MC_SLUG2GROUP_COUNT; i++)
        if (!strcmp(MC_SLUG2GROUP[i * 2], buf)) return json_string(MC_SLUG2GROUP[i * 2 + 1]);
    return json_string("");
}

json_t *w30_modelscata_4ee086_group_providers(const json_t *slugs) {
    json_t *out = json_array();
    char (*present)[MC_SLUG_CAP] = NULL, (*seen)[MC_SLUG_CAP] = NULL;
    const char **ppresent = NULL;               /* row-aligned views for lookups */
    size_t n, i, j, np = 0, ns = 0;
    const char *emitted[64];
    size_t nemitted = 0;
    size_t *gcount = NULL;

    if (!slugs || slugs->type != JSON_ARRAY) return out;
    n = json_len(slugs);
    if (n == 0) return out;
    if (n > 4096) n = 4096;                      /* bounded: no unbounded walk */
    present = (char (*)[MC_SLUG_CAP])calloc(n, MC_SLUG_CAP);
    seen = (char (*)[MC_SLUG_CAP])calloc(n, MC_SLUG_CAP);
    ppresent = (const char **)calloc(n, sizeof *ppresent);
    gcount = (size_t *)calloc(MC_GROUP_COUNT ? MC_GROUP_COUNT : 1, sizeof(size_t));
    if (!present || !seen || !ppresent || !gcount) {
        free(present); free(seen); free(ppresent); free(gcount);
        return out;
    }

    for (i = 0; i < n; i++) {
        char buf[MC_SLUG_CAP];
        int dup = 0;
        mc_norm(json_get(slugs, i), buf);
        if (!buf[0]) continue;
        for (j = 0; j < np; j++) if (!strcmp(ppresent[j], buf)) { dup = 1; break; }
        if (dup) continue;
        memcpy(present[np], buf, MC_SLUG_CAP);
        ppresent[np] = present[np];
        np++;
    }
    for (i = 0; i < MC_GROUP_COUNT; i++) {
        for (j = 0; j < MC_GROUPS[i].n; j++)
            if (mc_in(ppresent, np, MC_GROUPS[i].members[j])) gcount[i]++;
    }

    for (i = 0; i < n; i++) {
        char buf[MC_SLUG_CAP];
        int dup = 0, is_group = 0;
        size_t gi = 0, ndups = 0;
        mc_norm(json_get(slugs, i), buf);
        if (!buf[0]) continue;
        for (j = 0; j < ns; j++) if (!strcmp(seen[j], buf)) { dup = 1; break; }
        if (dup) continue;
        memcpy(seen[ns++], buf, MC_SLUG_CAP);

        for (gi = 0; gi < MC_SLUG2GROUP_COUNT; gi++)
            if (!strcmp(MC_SLUG2GROUP[gi * 2], buf)) { is_group = 1; break; }
        if (!is_group) {
            json_t *row = json_object();
            json_set(row, "kind", json_string("single"));
            json_set(row, "slug", json_string(buf));
            json_append(out, row);
            continue;
        }
        {
            const char *gid = MC_SLUG2GROUP[gi * 2 + 1];
            size_t g = 0;
            for (g = 0; g < MC_GROUP_COUNT; g++)
                if (!strcmp(MC_GROUPS[g].gid, gid)) break;
            if (g == MC_GROUP_COUNT) continue;
            for (j = 0; j < nemitted; j++)
                if (!strcmp(emitted[j], gid)) { ndups = 1; break; }
            if (ndups) continue;
            if (nemitted < sizeof emitted / sizeof emitted[0]) emitted[nemitted++] = gid;
            if (gcount[g] <= 1) {
                json_t *row = json_object();
                json_set(row, "kind", json_string("single"));
                json_set(row, "slug", json_string(buf));
                json_append(out, row);
            } else {
                json_t *row = json_object(), *mem = json_array();
                for (j = 0; j < MC_GROUPS[g].n; j++)
                    if (mc_in(ppresent, np, MC_GROUPS[g].members[j]))
                        json_append(mem, json_string(MC_GROUPS[g].members[j]));
                json_set(row, "kind", json_string("group"));
                json_set(row, "group_id", json_string(gid));
                json_set(row, "label", json_string(MC_GROUPS[g].label));
                json_set(row, "description", json_string(MC_GROUPS[g].desc));
                json_set(row, "members", mem);
                json_append(out, row);
            }
        }
    }
    free(present);
    free(seen);
    free(ppresent);
    free(gcount);
    return out;
}

/* ---- predicates and lookups ------------------------------------------- */

json_t *w30_modelscata_4ee086_plugin_provider_enters_picker(const json_t *name) {
    const char *k = mc_key(name);
    return json_bool(!mc_in((const char *const *)MC_BUILTIN_SLUGS, MC_BUILTIN_SLUGS_N, k));
}

json_t *w30_modelscata_4ee086_provider_models(const json_t *slug) {
    const char *k = mc_key(slug);
    size_t i;
    for (i = 0; i < MC_ROW_COUNT; i++)
        if (!strcmp(MC_ROW_SLUGS[i], k)) return mc_str_array(MC_ROW_IDS[i], MC_ROW_LEN[i]);
    return json_null();
}

json_t *w30_modelscata_4ee086_provider_label(const json_t *slug) {
    const char *k = mc_key(slug);
    size_t i;
    for (i = 0; i < MC_LABEL_COUNT; i++)
        if (!strcmp(MC_LABELS[i * 2], k)) return json_string(MC_LABELS[i * 2 + 1]);
    return json_null();
}

json_t *w30_modelscata_4ee086_provider_alias(const json_t *key) {
    const char *k = mc_key(key);
    size_t i;
    for (i = 0; i < MC_ALIAS_COUNT; i++)
        if (!strcmp(MC_ALIASES[i * 2], k)) return json_string(MC_ALIASES[i * 2 + 1]);
    return json_null();
}

json_t *w30_modelscata_4ee086_canonical_provider_row(const json_t *slug) {
    const char *k = mc_key(slug);
    size_t i;
    for (i = 0; i < MC_PROV_COUNT; i++) {
        if (strcmp(MC_PROV[i * 3], k)) continue;
        {
            json_t *row = json_array();
            json_append(row, json_string(MC_PROV[i * 3]));
            json_append(row, json_string(MC_PROV[i * 3 + 1]));
            json_append(row, json_string(MC_PROV[i * 3 + 2]));
            return row;
        }
    }
    return json_null();
}

json_t *w30_modelscata_4ee086_group_members(const json_t *gid) {
    const char *k = mc_key(gid);
    size_t i;
    for (i = 0; i < MC_GROUP_COUNT; i++) {
        if (strcmp(MC_GROUPS[i].gid, k)) continue;
        {
            json_t *row = json_array();
            json_append(row, json_string(MC_GROUPS[i].label));
            json_append(row, json_string(MC_GROUPS[i].desc));
            json_append(row, mc_str_array(MC_GROUPS[i].members, MC_GROUPS[i].n));
            return row;
        }
    }
    return json_null();
}
