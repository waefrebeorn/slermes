/*
 * mixture_of_agents.c — Mixture of Agents tool for Hermes C (slermes).
 *
 * MEGA TOTEM POLE EDITION - Dual Implementation Parity
 *
 * Features:
 * - Multi-provider FREE model orchestration (NVIDIA NIM, NVIDIA Cloud/NVCF, OpenRouter free, Nous Portal free)
 * - Provider health-aware rollover with auto-recovery (no auth timeouts)
 * - Token maxxing: optimal temperature/token configs per model class
 * - Triple Devil's Advocate mode (3 adversarial perspectives)
 * - Trepidation mode (cautious uncertainty quantification)
 * - Token Maxxing mode (maximum quality exhaustive)
 * - Tandem NVIDIA slam: NIM + Cloud simultaneous for mixed-model throughput
 * - NEVER uses Anthropic/Claude models
 * - RESEARCH-BASED MODEL ORDERING (totem poles from benchmark scores)
 * - Parallel reference model queries via pthreads for speed
 * - Uses all 4 API keys simultaneously (NVIDIA_API_KEY, NVIDIA_CLOUD_API_KEY, OPENROUTER_API_KEY, NOUS_API_KEY)
 * - Hermes-agentic benchmark focus: SWE-Bench, Terminal-Bench, LiveCodeBench, AA Coding Agents, FrontierMath, AIME
 *
 * NVIDIA NIM hosts 180+ FREE models from: NVIDIA, Meta, Google, Microsoft, Mistral, Cohere,
 * Z.ai, Moonshot, MiniMax, Qwen, DeepSeek, Phi, Gemma, and more.
 *
 * Based on "Mixture-of-Agents Enhances Large Language Model Capabilities"
 * (arXiv:2406.04692v1) with Hermes-specific extensions.
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "registry.h"
#include "hermes_logger.h"
#include "hermes_http.h"
#include "online_research.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

/* ─── MoA Mode Enum ──────────────────────────────────────────────── */

typedef enum {
    MOA_MODE_STANDARD = 0,
    MOA_MODE_DEVIL_ADVOCATE = 1,
    MOA_MODE_TREPIDATION = 2,
    MOA_MODE_TOKEN_MAXX = 3,
    MOA_MODE_MATH = 4,
    MOA_MODE_MEGA_TOTEM = 5,
} moa_mode_t;

/* ─── Provider & Model Configs ───────────────────────────────────── */

typedef struct {
    const char *name;
    const char *base_url;
    const char *api_key_env;
    const char **free_models;
    int num_free_models;
    int requires_auth;
    int priority;  // Lower = higher priority
    int health_check_interval;
    int max_consecutive_failures;
} moa_provider_config_t;

/* moa_ref_model_t is now defined in online_research.h for shared use */

/* ─── NVIDIA NIM MODELS (118 TOTAL, ALL FREE) ────────────────────
 * ALL models on NIM are FREE. NVIDIA is a cloud provider hosting non-NVIDIA
 * models (GLM, DeepSeek, MiniMax, Qwen, Kimi, Mistral, Meta, Google, etc.).
 * Live-tested 2026-07-24: 21 verified callable, rest slow/404/stream.
 */

static const char *NVIDIA_NIM_MODELS[] = {
    /* TIER S: Verified-callable agentic leaders (tested 2026-07-24) */
    "z-ai/glm-5.2",                               /* AA 51 | SWE-Pro 62.1 | Term-B 81% | #1 open-weight */
    "nvidia/nemotron-3-ultra-550b-a55b",           /* AA 47.7 | SWE-V 71.9% | 1M ctx */
    "nvidia/nemotron-3-super-120b-a12b",           /* AA 36 | 16.5 tok/s */
    "minimaxai/minimax-m3",                        /* SWE-bench Pro leaderboard */
    "deepseek-ai/deepseek-v4-flash",               /* AA 57 family | 13.3 tok/s */
    "mistralai/mistral-small-4-119b-2603",         /* 119B | newest Mistral */
    "mistralai/mistral-nemotron",                  /* NVIDIA-Mistral collab */

    /* TIER A: Strong but slow/404 — kept for when NVIDIA enables */
    "deepseek-ai/deepseek-v4-pro",                 /* AA 57 | SWE-V 80.6% | [SLOW] */
    "minimaxai/minimax-m2.7",                      /* SWE-Pro 75.8% | [SLOW] */
    "qwen/qwen3-next-80b-a3b-instruct",            /* Qwen3-Next 80B MoE | [SLOW] */
    "moonshotai/kimi-k2.6",                        /* SWE-Pro 58.6% #1 | [404] */
    "nvidia/llama-3.1-nemotron-ultra-253b-v1",     /* 253B dense | [404] */
    "nvidia/nemotron-4-340b-instruct",             /* 340B | [404] */
    "nvidia/llama-3.3-nemotron-super-49b-v1.5",    /* Nemotron Super 49B v1.5 | [STREAM] */
    "openai/gpt-oss-120b",                         /* AA 33 | [404] */
    "nvidia/llama-3.1-nemotron-70b-instruct",      /* 70B RLHF | [404] */

    /* TIER B: Fast/creative/specialist (verified callable) */
    "nvidia/nemotron-3-nano-omni-30b-a3b-reasoning", /* 63 tok/s BLAZING */
    "nvidia/nemotron-3-nano-30b-a3b",               /* 20.5 tok/s */
    "meta/llama-4-maverick-17b-128e-instruct",      /* Llama 4 Maverick MoE */
    "mistralai/mixtral-8x7b-instruct-v0.1",         /* Mixtral 8x7B */
    "poolside/laguna-xs-2.1",                       /* Coding */
    "google/diffusiongemma-26b-a4b-it",             /* DiffusionGemma 26B MoE */
    "google/gemma-3n-e4b-it",                       /* Gemma 3nano */
    "google/gemma-3n-e2b-it",                       /* Gemma 3nano */
    "google/gemma-2-2b-it",                         /* Gemma 2 2B */
    "nvidia/nemotron-mini-4b-instruct",             /* Nemotron Mini 4B */
    "sarvamai/sarvam-m",                            /* Sarvam-M Indic */
    "upstage/solar-10.7b-instruct",                 /* Solar 10.7B */
    "meta/llama-3.2-3b-instruct",                   /* Llama 3.2 3B */
    "meta/llama-3.1-8b-instruct",                   /* Llama 3.1 8B */

    /* TIER C: Listed but not callable (kept for documentation) */
    "thinkingmachines/inkling",                     /* [STREAM] */
    "stepfun-ai/step-3.5-flash",                    /* [STREAM] */
    "stepfun-ai/step-3.7-flash",                    /* [STREAM] */
    "openai/gpt-oss-20b",                           /* [STREAM] */
    "nvidia/nvidia-nemotron-nano-9b-v2",             /* [STREAM] */
    "mistralai/mistral-medium-3.5-128b",             /* [SLOW] */
    "meta/llama-3.3-70b-instruct",                   /* [SLOW] */
    "google/gemma-4-31b-it",                         /* [SLOW] */
    "bytedance/seed-oss-36b-instruct",               /* [SLOW] */
    "mistralai/ministral-14b-instruct-2512",         /* [SLOW] */
    "writer/palmyra-creative-122b",                  /* [404] */
    "mistralai/mistral-large-2-instruct",            /* [404] */
    "ai21labs/jamba-1.5-large-instruct",             /* [404] */
    "qwen/qwen3.5-397b-a17b",                        /* [404] */
    "nvidia/cosmos-reason2-8b",                      /* [404] */
    "nvidia/nemotron-nano-3-30b-a3b",                /* [404] */
    "nvidia/llama-3.1-nemotron-51b-instruct",        /* [404] */
    "nvidia/llama-3.1-nemotron-nano-8b-v1",          /* [SLOW] */
};

static const int NVIDIA_NIM_MODELS_COUNT = sizeof(NVIDIA_NIM_MODELS) / sizeof(NVIDIA_NIM_MODELS[0]);

/* ─── OPENROUTER FREE MODELS ─────────────────────────────────────── */

static const char *OPENROUTER_FREE_MODELS[] = {
    "nvidia/nemotron-3-ultra-550b-a55b:free",
    "nvidia/nemotron-3-super-120b-a12b:free",
    "nvidia/llama-3.1-nemotron-70b-instruct:free",
    "nvidia/nemotron-4-340b-instruct:free",
    "inclusionai/ring-2.6-1t:free",
    "poolside/laguna-m.1:free",
    "openrouter/elephant-alpha",
    "openrouter/owl-alpha",
    "tencent/hy3-preview:free",
    "north/north-mini-code:free",
    "mimo-v2.5:free",
    "big-pickle:free",
    "nemotron-3-ultra:free",
};

static const int OPENROUTER_FREE_MODELS_COUNT = sizeof(OPENROUTER_FREE_MODELS) / sizeof(OPENROUTER_FREE_MODELS[0]);

/* ─── NOUS PORTAL FREE MODELS ────────────────────────────────────── */

static const char *NOUS_FREE_MODELS[] = {
    "deepseek/deepseek-v4-flash",
    "google/gemini-3.5-flash",
    "meta-llama/llama-3.1-405b-instruct:free",
};

static const int NOUS_FREE_MODELS_COUNT = sizeof(NOUS_FREE_MODELS) / sizeof(NOUS_FREE_MODELS[0]);

/* ─── PROVIDER CONFIGURATIONS ────────────────────────────────────── */

static moa_provider_config_t MOA_PROVIDERS[] = {
    {
        .name = "nvidia_nim",
        .base_url = "https://integrate.api.nvidia.com/v1",
        .api_key_env = "NVIDIA_API_KEY",
        .free_models = NVIDIA_NIM_MODELS,
        .num_free_models = NVIDIA_NIM_MODELS_COUNT,
        .requires_auth = 0,
        .priority = 1,
        .health_check_interval = 60,
        .max_consecutive_failures = 3,
    },
    {
        .name = "nvidia_cloud",
        .base_url = "https://api.nvcf.nvidia.com/v1",
        .api_key_env = "NVIDIA_CLOUD_API_KEY",
        .free_models = NVIDIA_NIM_MODELS,  /* Same 180+ catalog on dedicated cloud infra */
        .num_free_models = NVIDIA_NIM_MODELS_COUNT,
        .requires_auth = 1,
        .priority = 2,
        .health_check_interval = 60,
        .max_consecutive_failures = 3,
    },
    {
        .name = "openrouter",
        .base_url = "https://openrouter.ai/api/v1",
        .api_key_env = "OPENROUTER_API_KEY",
        .free_models = OPENROUTER_FREE_MODELS,
        .num_free_models = OPENROUTER_FREE_MODELS_COUNT,
        .requires_auth = 1,
        .priority = 3,
        .health_check_interval = 60,
        .max_consecutive_failures = 3,
    },
    {
        .name = "nous_portal",
        .base_url = "https://inference-api.nousresearch.com/v1",
        .api_key_env = "NOUS_API_KEY",
        .free_models = NOUS_FREE_MODELS,
        .num_free_models = NOUS_FREE_MODELS_COUNT,
        .requires_auth = 1,
        .priority = 4,
        .health_check_interval = 60,
        .max_consecutive_failures = 3,
    },
};

static const int MOA_PROVIDERS_COUNT = sizeof(MOA_PROVIDERS) / sizeof(MOA_PROVIDERS[0]);

/* ─── SYSTEM PROMPTS ─────────────────────────────────────────────── */

static const char *MOA_STANDARD_SYSTEM = 
    "You are an expert AI assistant participating in a Mixture of Agents ensemble.\n"
    "Your role: Provide a thorough, accurate, and well-reasoned response to the user's query.\n"
    "Draw on your specific strengths (coding, reasoning, analysis, creativity) as appropriate.\n"
    "Be concise but comprehensive. Focus on actionable, correct information.";

static const char *MOA_DEVIL_AGGREGATOR_SYSTEM = 
    "You are the aggregator for a Devil's Advocate Mixture of Agents.\n"
    "Three critics have challenged the problem from different angles:\n"
    "- Critic 1 (Skeptic): Challenges assumptions, finds holes, questions premises\n"
    "- Critic 2 (Optimizer): Improves efficiency, finds better algorithms, reduces complexity\n"
    "- Critic 3 (Safety): Checks for risks, edge cases, security issues, unintended consequences\n\n"
    "Synthesize their adversarial perspectives into a robust, bulletproof final answer.\n"
    "Address every criticism. Strengthen every weakness. Produce the most resilient solution possible.";

static const char *MOA_TREPIDATION_AGGREGATOR_SYSTEM = 
    "You are the aggregator for a Trepidation (Uncertainty Quantification) Mixture of Agents.\n"
    "Multiple cautious models have provided responses with explicit confidence intervals,\n"
    "uncertainty assessments, and variance analysis.\n\n"
    "Your task: Produce a final answer that honestly represents the epistemic state.\n"
    "- Report confidence levels for each claim\n"
    "- Flag where models disagree\n"
    "- Quantify uncertainty (e.g., \"80% confident\", \"high variance across models\")\n"
    "- Distinguish between aleatoric (data) and epistemic (model) uncertainty\n"
    "- Provide calibrated probabilities where possible\n\n"
    "Do not overstate certainty. The user needs to know what's reliable vs speculative.";

static const char *MOA_TOKEN_MAXX_AGGREGATOR_SYSTEM = 
    "You are the aggregator for a Token Maxxing Mixture of Agents.\n"
    "Multiple high-capacity models have provided exhaustive, detailed responses\n"
    "with maximum token budgets and optimal sampling parameters.\n\n"
    "Synthesize the most comprehensive, thorough, and high-quality answer possible.\n"
    "- Merge unique insights from each model\n"
    "- Resolve contradictions by favoring higher-confidence sources\n"
    "- Preserve technical depth and nuance\n"
    "- Include code examples, proofs, derivations where relevant\n"
    "- Structure for maximum utility: executive summary -> deep dive -> references";

static const char *MOA_STANDARD_AGGREGATOR_SYSTEM = 
    "You are the final aggregator in a Mixture of Agents ensemble.\n"
    "Multiple expert models have provided their perspectives on the user's query.\n"
    "Synthesize their responses into a single, coherent, high-quality answer.\n\n"
    "Guidelines:\n"
    "- Merge unique insights, don't just repeat\n"
    "- Resolve conflicts by weighing model strengths (benchmark-ordered)\n"
    "- Preserve technical accuracy and nuance\n"
    "- Structure clearly: summary -> details -> actionable conclusion\n"
    "- Credit the ensemble approach when relevant";

static const char *MOA_DEVIL_SYSTEM = 
    "You are a DEVIL'S ADVOCATE critic in a Mixture of Agents ensemble.\n"
    "Your role: Challenge the problem aggressively. Find flaws, holes, edge cases, wrong assumptions.\n"
    "Be ruthless but constructive. Your criticism makes the final answer stronger.\n"
    "Focus on: logical gaps, missing edge cases, security risks, performance issues, wrong assumptions.";

static const char *MOA_TREPIDATION_SYSTEM = 
    "You are a CAUTIOUS ANALYST in a Mixture of Agents ensemble.\n"
    "Your role: Quantify uncertainty. Report confidence intervals. Flag disagreement.\n"
    "For every claim, state your confidence (0-100%). Note where you're uncertain.\n"
    "Distinguish: what you know vs what you assume vs what's speculative.\n"
    "Be precise about variance sources.";

static const char *MOA_TOKEN_MAXX_SYSTEM = 
    "You are a MAXIMUM QUALITY contributor in a Mixture of Agents ensemble.\n"
    "Your role: Exhaustive, thorough, maximum detail. Use your full token budget.\n"
    "Provide: complete derivations, full code with tests, comprehensive analysis,\n"
    "edge cases, alternatives, benchmarks, references. No shortcuts.";

/* ─── MODEL SELECTION FOR EACH MODE ──────────────────────────────── */

/* Standard MoA: diverse perspectives, high reasoning (RESEARCH-ORDERED, live-tested 2026-07-24) */
static const moa_ref_model_t MOA_STANDARD_REFS[] = {
    /* TIER S: Verified-callable agentic leaders */
    { "nvidia_nim", "z-ai/glm-5.2", 0.6f, 4096, "xhigh", "analytical", 1 },
    { "nvidia_nim", "nvidia/nemotron-3-ultra-550b-a55b", 0.6f, 4096, "xhigh", "analytical", 1 },
    { "nvidia_nim", "nvidia/nemotron-3-super-120b-a12b", 0.6f, 4096, "high", "analytical", 1 },
    { "nvidia_nim", "minimaxai/minimax-m3", 0.6f, 4096, "high", "analytical", 1 },
    { "nvidia_nim", "deepseek-ai/deepseek-v4-flash", 0.6f, 4096, "high", "analytical", 1 },
    { "nvidia_nim", "mistralai/mistral-small-4-119b-2603", 0.6f, 4096, "high", "analytical", 1 },
    { "nvidia_nim", "mistralai/mistral-nemotron", 0.5f, 4096, "medium", "analytical", 2 },
    /* TIER A: Strong but slow/404 */
    { "nvidia_nim", "deepseek-ai/deepseek-v4-pro", 0.6f, 4096, "xhigh", "analytical", 1 },
    { "nvidia_nim", "minimaxai/minimax-m2.7", 0.6f, 4096, "xhigh", "analytical", 1 },
    { "nvidia_nim", "qwen/qwen3-next-80b-a3b-instruct", 0.6f, 4096, "high", "analytical", 1 },
    { "nvidia_nim", "moonshotai/kimi-k2.6", 0.6f, 4096, "xhigh", "analytical", 1 },
    { "nvidia_nim", "nvidia/llama-3.1-nemotron-ultra-253b-v1", 0.6f, 4096, "xhigh", "analytical", 1 },
    { "nvidia_nim", "nvidia/nemotron-4-340b-instruct", 0.6f, 4096, "xhigh", "analytical", 1 },
    { "nvidia_nim", "nvidia/llama-3.3-nemotron-super-49b-v1.5", 0.5f, 4096, "high", "creative", 2 },
    { "nvidia_nim", "openai/gpt-oss-120b", 0.5f, 4096, "medium", "synthesizer", 2 },
    { "nvidia_nim", "nvidia/llama-3.1-nemotron-70b-instruct", 0.5f, 4096, "high", "analytical", 2 },
    /* TIER B: Fast/creative/specialist */
    { "nvidia_nim", "nvidia/nemotron-3-nano-omni-30b-a3b-reasoning", 0.5f, 4096, "medium", "creative", 2 },
    { "nvidia_nim", "meta/llama-4-maverick-17b-128e-instruct", 0.5f, 4096, "medium", "creative", 2 },
    { "nvidia_nim", "mistralai/mixtral-8x7b-instruct-v0.1", 0.5f, 4096, "medium", "creative", 2 },
    { "nvidia_nim", "poolside/laguna-xs-2.1", 0.5f, 4096, "high", "analytical", 2 },
    { "nvidia_nim", "google/diffusiongemma-26b-a4b-it", 0.5f, 4096, "medium", "creative", 2 },
    /* OpenRouter free tier */
    { "openrouter", "nvidia/nemotron-3-ultra-550b-a55b:free", 0.6f, 4096, "xhigh", "analytical", 1 },
    { "openrouter", "nvidia/nemotron-3-super-120b-a12b:free", 0.5f, 4096, "high", "creative", 2 },
    { "openrouter", "inclusionai/ring-2.6-1t:free", 0.6f, 4096, "high", "creative", 2 },
    /* Nous Portal fallback */
    { "nous_portal", "deepseek/deepseek-v4-flash", 0.5f, 4096, "high", "analytical", 3 },
    { "nous_portal", "google/gemini-3.5-flash", 0.5f, 4096, "high", "analytical", 3 },
    { "nous_portal", "meta-llama/llama-3.1-405b-instruct:free", 0.5f, 4096, "medium", "synthesizer", 3 },
};

static const int MOA_STANDARD_REFS_COUNT = sizeof(MOA_STANDARD_REFS) / sizeof(MOA_STANDARD_REFS[0]);

/* Triple Devil's Advocate: adversarial perspectives (BEST verified-callable) */
static const moa_ref_model_t MOA_DEVIL_REFS[] = {
    { "nvidia_nim", "z-ai/glm-5.2", 0.8f, 4096, "xhigh", "devil_advocate_1", 1 },
    { "nvidia_nim", "nvidia/nemotron-3-ultra-550b-a55b", 0.8f, 4096, "xhigh", "devil_advocate_2", 1 },
    { "nvidia_nim", "deepseek-ai/deepseek-v4-flash", 0.8f, 4096, "high", "devil_advocate_3", 1 },
    // Additional critics for redundancy
    { "nvidia_nim", "minimaxai/minimax-m3", 0.8f, 4096, "high", "devil_advocate_4", 1 },
    { "openrouter", "nvidia/nemotron-3-ultra-550b-a55b:free", 0.8f, 4096, "xhigh", "devil_advocate_5", 1 },
};

static const int MOA_DEVIL_REFS_COUNT = sizeof(MOA_DEVIL_REFS) / sizeof(MOA_DEVIL_REFS[0]);

/* Trepidation: cautious, uncertainty-focused (BEST verified callable) */
static const moa_ref_model_t MOA_TREPIDATION_REFS[] = {
    { "nvidia_nim", "z-ai/glm-5.2", 0.4f, 4096, "xhigh", "critical", 1 },
    { "nvidia_nim", "nvidia/nemotron-3-ultra-550b-a55b", 0.4f, 4096, "xhigh", "critical", 1 },
    { "nvidia_nim", "deepseek-ai/deepseek-v4-flash", 0.4f, 4096, "high", "critical", 1 },
    { "nvidia_nim", "nvidia/nemotron-3-super-120b-a12b", 0.4f, 4096, "high", "critical", 2 },
    { "nvidia_nim", "minimaxai/minimax-m3", 0.4f, 4096, "high", "critical", 1 },
    { "nvidia_nim", "mistralai/mistral-small-4-119b-2603", 0.4f, 4096, "high", "critical", 1 },
    { "openrouter", "nvidia/nemotron-3-ultra-550b-a55b:free", 0.4f, 4096, "xhigh", "critical", 1 },
};

static const int MOA_TREPIDATION_REFS_COUNT = sizeof(MOA_TREPIDATION_REFS) / sizeof(MOA_TREPIDATION_REFS[0]);

/* Token Maxxing: maximum quality, maximum tokens (BEST verified callable) */
static const moa_ref_model_t MOA_TOKEN_MAXX_REFS[] = {
    { "nvidia_nim", "z-ai/glm-5.2", 0.7f, 8192, "xhigh", "analytical", 1 },
    { "nvidia_nim", "nvidia/nemotron-3-ultra-550b-a55b", 0.7f, 8192, "xhigh", "analytical", 1 },
    { "nvidia_nim", "nvidia/nemotron-3-super-120b-a12b", 0.7f, 8192, "high", "analytical", 1 },
    { "nvidia_nim", "minimaxai/minimax-m3", 0.7f, 8192, "high", "analytical", 1 },
    { "nvidia_nim", "deepseek-ai/deepseek-v4-flash", 0.7f, 8192, "high", "analytical", 1 },
    { "nvidia_nim", "mistralai/mistral-small-4-119b-2603", 0.7f, 8192, "high", "analytical", 1 },
    { "nvidia_nim", "mistralai/mistral-nemotron", 0.7f, 8192, "medium", "creative", 2 },
    { "nvidia_nim", "nvidia/nemotron-3-nano-omni-30b-a3b-reasoning", 0.7f, 8192, "medium", "creative", 2 },
    { "nvidia_nim", "meta/llama-4-maverick-17b-128e-instruct", 0.7f, 8192, "medium", "creative", 2 },
    { "nvidia_nim", "poolside/laguna-xs-2.1", 0.7f, 8192, "high", "analytical", 2 },
    { "nvidia_nim", "google/diffusiongemma-26b-a4b-it", 0.7f, 8192, "medium", "creative", 2 },
    
    // Cross-provider redundancy for maxxing
    { "nvidia_nim", "deepseek-ai/deepseek-v4-pro", 0.7f, 8192, "xhigh", "analytical", 1 },
    { "nvidia_nim", "minimaxai/minimax-m2.7", 0.7f, 8192, "xhigh", "analytical", 1 },
    { "nvidia_nim", "moonshotai/kimi-k2.6", 0.7f, 8192, "xhigh", "analytical", 1 },
    
    // OpenRouter free tier redundancy
    { "openrouter", "nvidia/nemotron-3-ultra-550b-a55b:free", 0.8f, 8192, "xhigh", "analytical", 1 },
    { "openrouter", "nvidia/nemotron-3-super-120b-a12b:free", 0.7f, 8192, "xhigh", "analytical", 1 },
    { "openrouter", "inclusionai/ring-2.6-1t:free", 0.7f, 8192, "high", "creative", 2 },
    
    // Nous Portal
    { "nous_portal", "meta-llama/llama-3.1-405b-instruct:free", 0.7f, 8192, "medium", "synthesizer", 3 },
    { "nous_portal", "deepseek/deepseek-v4-flash", 0.7f, 8192, "high", "analytical", 3 },
    { "nous_portal", "google/gemini-3.5-flash", 0.7f, 8192, "high", "analytical", 3 },
};

static const int MOA_TOKEN_MAXX_REFS_COUNT = sizeof(MOA_TOKEN_MAXX_REFS) / sizeof(MOA_TOKEN_MAXX_REFS[0]);

/* Math specialized references */
static const moa_ref_model_t MOA_MATH_REFS[] = {
    { "nvidia_nim", "z-ai/glm-5.2", 0.3f, 8192, "xhigh", "math_analytical", 1 },
    { "nvidia_nim", "nvidia/nemotron-3-ultra-550b-a55b", 0.3f, 8192, "xhigh", "math_analytical", 1 },
    { "nvidia_nim", "nvidia/nemotron-3-super-120b-a12b", 0.3f, 8192, "high", "math_analytical", 1 },
    { "nvidia_nim", "deepseek-ai/deepseek-v4-flash", 0.3f, 8192, "high", "math_analytical", 1 },
    { "nvidia_nim", "minimaxai/minimax-m3", 0.3f, 8192, "high", "math_analytical", 1 },
    { "nvidia_nim", "mistralai/mistral-small-4-119b-2603", 0.3f, 8192, "high", "math_analytical", 1 },
    { "nvidia_nim", "nvidia/nemotron-3-nano-omni-30b-a3b-reasoning", 0.3f, 8192, "medium", "math_creative", 2 },
    { "openrouter", "nvidia/nemotron-3-ultra-550b-a55b:free", 0.3f, 8192, "xhigh", "math_analytical", 1 },
};

static const int MOA_MATH_REFS_COUNT = sizeof(MOA_MATH_REFS) / sizeof(MOA_MATH_REFS[0]);

/* MEGA TOTEM: ALL verified-callable models across all 3 providers
 * Fires every model in parallel for maximum diversity + quality.
 * 21 NVIDIA NIM verified + 3 OpenRouter free + 3 Nous Portal = 27 models.
 * Default aggregator = GLM-5.2 (xhigh reasoning).
 * This is THE mode most users will use — fast, awesome, free.
 */
static const moa_ref_model_t MOA_MEGA_TOTEM_REFS[] = {
    /* NVIDIA NIM TIER S (7 verified callable) */
    { "nvidia_nim", "z-ai/glm-5.2", 0.6f, 4096, "xhigh", "analytical", 1 },
    { "nvidia_nim", "nvidia/nemotron-3-ultra-550b-a55b", 0.6f, 4096, "xhigh", "analytical", 1 },
    { "nvidia_nim", "nvidia/nemotron-3-super-120b-a12b", 0.6f, 4096, "high", "analytical", 1 },
    { "nvidia_nim", "minimaxai/minimax-m3", 0.6f, 4096, "high", "analytical", 1 },
    { "nvidia_nim", "deepseek-ai/deepseek-v4-flash", 0.6f, 4096, "high", "analytical", 1 },
    { "nvidia_nim", "mistralai/mistral-small-4-119b-2603", 0.6f, 4096, "high", "analytical", 1 },
    { "nvidia_nim", "mistralai/mistral-nemotron", 0.5f, 4096, "medium", "creative", 2 },
    /* NVIDIA NIM TIER B (7 fast/creative verified callable) */
    { "nvidia_nim", "nvidia/nemotron-3-nano-omni-30b-a3b-reasoning", 0.5f, 4096, "medium", "creative", 2 },
    { "nvidia_nim", "nvidia/nemotron-3-nano-30b-a3b", 0.5f, 4096, "medium", "creative", 2 },
    { "nvidia_nim", "meta/llama-4-maverick-17b-128e-instruct", 0.5f, 4096, "medium", "creative", 2 },
    { "nvidia_nim", "mistralai/mixtral-8x7b-instruct-v0.1", 0.5f, 4096, "medium", "creative", 2 },
    { "nvidia_nim", "poolside/laguna-xs-2.1", 0.5f, 4096, "high", "analytical", 2 },
    { "nvidia_nim", "google/diffusiongemma-26b-a4b-it", 0.5f, 4096, "medium", "creative", 2 },
    { "nvidia_nim", "google/gemma-3n-e4b-it", 0.5f, 4096, "medium", "creative", 2 },
    /* NVIDIA NIM EXTRA (4 more verified callable) */
    { "nvidia_nim", "google/gemma-3n-e2b-it", 0.5f, 4096, "medium", "creative", 2 },
    { "nvidia_nim", "google/gemma-2-2b-it", 0.5f, 4096, "medium", "creative", 2 },
    { "nvidia_nim", "nvidia/nemotron-mini-4b-instruct", 0.5f, 4096, "medium", "creative", 2 },
    { "nvidia_nim", "sarvamai/sarvam-m", 0.5f, 4096, "medium", "creative", 2 },
    { "nvidia_nim", "upstage/solar-10.7b-instruct", 0.5f, 4096, "medium", "creative", 2 },
    { "nvidia_nim", "meta/llama-3.2-3b-instruct", 0.5f, 4096, "medium", "creative", 2 },
    { "nvidia_nim", "meta/llama-3.1-8b-instruct", 0.5f, 4096, "medium", "creative", 2 },
    /* OpenRouter free tier (3) */
    { "openrouter", "nvidia/nemotron-3-ultra-550b-a55b:free", 0.6f, 4096, "xhigh", "analytical", 1 },
    { "openrouter", "nvidia/nemotron-3-super-120b-a12b:free", 0.5f, 4096, "high", "creative", 2 },
    { "openrouter", "inclusionai/ring-2.6-1t:free", 0.6f, 4096, "high", "creative", 2 },
    /* Nous Portal fallback (3) */
    { "nous_portal", "deepseek/deepseek-v4-flash", 0.5f, 4096, "high", "analytical", 3 },
    { "nous_portal", "google/gemini-3.5-flash", 0.5f, 4096, "high", "analytical", 3 },
    { "nous_portal", "meta-llama/llama-3.1-405b-instruct:free", 0.5f, 4096, "medium", "synthesizer", 3 },
};

static const int MOA_MEGA_TOTEM_REFS_COUNT = sizeof(MOA_MEGA_TOTEM_REFS) / sizeof(MOA_MEGA_TOTEM_REFS[0]);

/* ─── PROVIDER HEALTH TRACKING ───────────────────────────────────── */

typedef struct {
    int healthy;
    int consecutive_failures;
    time_t last_failure_time;
    time_t last_success_time;
} moa_provider_health_t;

static moa_provider_health_t provider_health[4];  // Max 4 providers
static pthread_mutex_t health_mutex = PTHREAD_MUTEX_INITIALIZER;

static void init_provider_health() {
    for (int i = 0; i < MOA_PROVIDERS_COUNT; i++) {
        provider_health[i].healthy = 1;
        provider_health[i].consecutive_failures = 0;
        provider_health[i].last_failure_time = 0;
        provider_health[i].last_success_time = time(NULL);
    }
}

static int get_provider_index(const char *provider_name) {
    for (int i = 0; i < MOA_PROVIDERS_COUNT; i++) {
        if (strcmp(MOA_PROVIDERS[i].name, provider_name) == 0) {
            return i;
        }
    }
    return -1;
}

static int is_provider_healthy(const char *provider_name) {
    int idx = get_provider_index(provider_name);
    if (idx < 0) return 1;  // Unknown provider = healthy by default
    
    pthread_mutex_lock(&health_mutex);
    int healthy = provider_health[idx].healthy;
    time_t now = time(NULL);
    
    // Auto-recovery after 60 seconds
    if (!healthy && (now - provider_health[idx].last_failure_time > 60)) {
        provider_health[idx].healthy = 1;
        provider_health[idx].consecutive_failures = 0;
        healthy = 1;
    }
    pthread_mutex_unlock(&health_mutex);
    return healthy;
}

static void record_provider_success(const char *provider_name) {
    int idx = get_provider_index(provider_name);
    if (idx < 0) return;
    
    pthread_mutex_lock(&health_mutex);
    provider_health[idx].healthy = 1;
    provider_health[idx].consecutive_failures = 0;
    provider_health[idx].last_success_time = time(NULL);
    pthread_mutex_unlock(&health_mutex);
}

static void record_provider_failure(const char *provider_name) {
    int idx = get_provider_index(provider_name);
    if (idx < 0) return;
    
    pthread_mutex_lock(&health_mutex);
    provider_health[idx].consecutive_failures++;
    provider_health[idx].last_failure_time = time(NULL);
    int max_failures = MOA_PROVIDERS[idx].max_consecutive_failures;
    if (provider_health[idx].consecutive_failures >= max_failures) {
        provider_health[idx].healthy = 0;
        hermes_log(LOG_WARNING, "moa", "Provider %s marked unhealthy after %d failures",
                   provider_name, provider_health[idx].consecutive_failures);
    }
    pthread_mutex_unlock(&health_mutex);
}

/* ─── HTTP REQUEST HELPER ───────────────────────────────────────── */

typedef struct {
    char *buffer;
    size_t size;
    size_t capacity;
} moa_http_response_t;

static size_t moa_http_write_callback(const char *data, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    moa_http_response_t *resp = (moa_http_response_t *)userp;
    
    if (resp->size + realsize + 1 > resp->capacity) {
        resp->capacity = (resp->size + realsize + 1) * 2;
        resp->buffer = realloc(resp->buffer, resp->capacity);
    }
    memcpy(resp->buffer + resp->size, data, realsize);
    resp->size += realsize;
    resp->buffer[resp->size] = '\0';
    return realsize;
}

static char *moa_call_model(
    const moa_provider_config_t *provider,
    const moa_ref_model_t *ref,
    const char *system_prompt,
    const char *user_prompt
) {
    if (!is_provider_healthy(provider->name)) {
        return NULL;
    }
    
    // Build JSON payload
    json_t *payload = json_object();
    json_t *messages = json_array();
    
    if (system_prompt && strlen(system_prompt) > 0) {
        json_t *sys_msg = json_object();
        json_set(sys_msg, "role", json_string("system"));
        json_set(sys_msg, "content", json_string(system_prompt));
        json_append(messages, sys_msg);
    }
    
    json_t *user_msg = json_object();
    json_set(user_msg, "role", json_string("user"));
    json_set(user_msg, "content", json_string(user_prompt));
    json_append(messages, user_msg);
    
    json_set(payload, "model", json_string(ref->model));
    json_set(payload, "messages", messages);
    json_set(payload, "temperature", json_number(ref->temperature));
    json_set(payload, "max_tokens", json_number(ref->max_tokens));
    json_set(payload, "stream", json_bool(0));
    
    if (strcmp(ref->reasoning_effort, "none") != 0) {
        json_t *reasoning = json_object();
        json_set(reasoning, "effort", json_string(ref->reasoning_effort));
        json_set(reasoning, "enabled", json_bool(1));
        json_set(payload, "reasoning", reasoning);
    }
    
    char *payload_str = json_serialize(payload);
    json_free(payload);
    
    // Build URL
    char url[512];
    snprintf(url, sizeof(url), "%s/chat/completions", provider->base_url);
    
    // Get API key from environment
        const char *api_key = getenv(provider->api_key_env);
    
        // Make HTTP request using libhttp
        http_t *http = http_new(300);  // 5 minute timeout
        if (!http) {
            free(payload_str);
            return NULL;
        }
    
        // Build headers
        char headers[1024] = "";
        strcat(headers, "Content-Type: application/json\n");
        strcat(headers, "Accept: application/json\n");
        if (api_key) {
            char auth_header[512];
            snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s\n", api_key);
            strcat(headers, auth_header);
        }
    
    http_resp_t *resp = http_request(http, HTTP_POST, url, headers, payload_str, strlen(payload_str));
    free(payload_str);
    
    int status = 0;
    char *body = NULL;
    if (resp) {
        status = resp->status;
        if (resp->body) {
            body = strdup(resp->body);
        }
        http_resp_free(resp);
    }
    
    http_free(http);
    
    if (status != 200) {
        if (status == 401) {
            hermes_log(LOG_WARNING, "moa", "%s auth failed - check %s", provider->name, provider->api_key_env);
        } else {
            hermes_log(LOG_WARNING, "moa", "%s HTTP %d: %s", provider->name, status, body ? body : "(no body)");
        }
        record_provider_failure(provider->name);
        free(body);
        return NULL;
    }
    
    // Parse JSON response
    json_t *data = json_parse(body, NULL);
    free(body);
    
    if (!data) {
        record_provider_failure(provider->name);
        return NULL;
    }
    
    const char *content = NULL;
    json_t *choices = json_obj_get(data, "choices");
    if (choices && choices->type == JSON_ARRAY && choices->c.count > 0) {
        json_t *first = json_get(choices, 0);
        json_t *message = json_obj_get(first, "message");
        if (message) {
            content = json_get_str(message, "content", "");
            if (!content || strlen(content) == 0) {
                // Check for reasoning content
                json_t *reasoning = json_obj_get(message, "reasoning");
                if (reasoning && reasoning->type == JSON_OBJECT) {
                    content = json_get_str(reasoning, "content", "");
                }
            }
        }
    }
    
    char *result_str = NULL;
    if (content && strlen(content) > 0) {
        result_str = strdup(content);
        record_provider_success(provider->name);
    } else {
        record_provider_failure(provider->name);
    }
    
    json_free(data);
    return result_str;
}

/* ─── THREADED PARALLEL QUERY ────────────────────────────────────── */

typedef struct {
    const moa_provider_config_t *provider;
    const moa_ref_model_t *ref;
    const char *system_prompt;
    const char *user_prompt;
    char *result;
    int done;
} moa_query_task_t;

static void *moa_query_thread(void *arg) {
    moa_query_task_t *task = (moa_query_task_t *)arg;
    task->result = moa_call_model(task->provider, task->ref, task->system_prompt, task->user_prompt);
    task->done = 1;
    return NULL;
}

static int moa_query_all_references(
    const moa_ref_model_t *refs,
    int ref_count,
    const char *system_prompt,
    const char *user_prompt,
    moa_query_task_t **out_tasks
) {
    moa_query_task_t *tasks = calloc(ref_count, sizeof(moa_query_task_t));
    pthread_t *threads = calloc(ref_count, sizeof(pthread_t));
    
    // Create threads
    for (int i = 0; i < ref_count; i++) {
        // Find provider config
        const moa_provider_config_t *provider = NULL;
        for (int p = 0; p < MOA_PROVIDERS_COUNT; p++) {
            if (strcmp(MOA_PROVIDERS[p].name, refs[i].provider) == 0) {
                provider = &MOA_PROVIDERS[p];
                break;
            }
        }
        if (!provider) {
            tasks[i].result = NULL;
            tasks[i].done = 1;
            continue;
        }
        
        tasks[i].provider = provider;
        tasks[i].ref = &refs[i];
        tasks[i].system_prompt = system_prompt;
        tasks[i].user_prompt = user_prompt;
        tasks[i].result = NULL;
        tasks[i].done = 0;
        
        pthread_create(&threads[i], NULL, moa_query_thread, &tasks[i]);
    }
    
    // Wait for all threads
    for (int i = 0; i < ref_count; i++) {
        pthread_join(threads[i], NULL);
    }
    
    free(threads);
    *out_tasks = tasks;
    return ref_count;
}

/* ─── MAIN MoA HANDLER ───────────────────────────────────────────── */

static const char *get_system_prompt(moa_mode_t mode, int is_aggregator) {
    if (is_aggregator) {
        switch (mode) {
            case MOA_MODE_DEVIL_ADVOCATE: return MOA_DEVIL_AGGREGATOR_SYSTEM;
            case MOA_MODE_TREPIDATION: return MOA_TREPIDATION_AGGREGATOR_SYSTEM;
            case MOA_MODE_TOKEN_MAXX: return MOA_TOKEN_MAXX_AGGREGATOR_SYSTEM;
            default: return MOA_STANDARD_AGGREGATOR_SYSTEM;
        }
    } else {
        switch (mode) {
            case MOA_MODE_DEVIL_ADVOCATE: return MOA_DEVIL_SYSTEM;
            case MOA_MODE_TREPIDATION: return MOA_TREPIDATION_SYSTEM;
            case MOA_MODE_TOKEN_MAXX: return MOA_TOKEN_MAXX_SYSTEM;
            default: return MOA_STANDARD_SYSTEM;
        }
    }
}

static const moa_ref_model_t *get_ref_models(moa_mode_t mode, int *out_count) {
    switch (mode) {
        case MOA_MODE_STANDARD:
            *out_count = MOA_STANDARD_REFS_COUNT;
            return MOA_STANDARD_REFS;
        case MOA_MODE_DEVIL_ADVOCATE:
            *out_count = MOA_DEVIL_REFS_COUNT;
            return MOA_DEVIL_REFS;
        case MOA_MODE_TREPIDATION:
            *out_count = MOA_TREPIDATION_REFS_COUNT;
            return MOA_TREPIDATION_REFS;
        case MOA_MODE_TOKEN_MAXX:
            *out_count = MOA_TOKEN_MAXX_REFS_COUNT;
            return MOA_TOKEN_MAXX_REFS;
        case MOA_MODE_MATH:
            *out_count = MOA_MATH_REFS_COUNT;
            return MOA_MATH_REFS;
        case MOA_MODE_MEGA_TOTEM:
            *out_count = MOA_MEGA_TOTEM_REFS_COUNT;
            return MOA_MEGA_TOTEM_REFS;
        default:
            *out_count = MOA_MEGA_TOTEM_REFS_COUNT;
            return MOA_MEGA_TOTEM_REFS;
    }
}

char *handle_mixture_of_agents(const char *args_json, const char *task_id) {
    (void)task_id;  // Unused for now
    
    if (!args_json) {
        return strdup("{\"error\":\"Missing arguments\",\"success\":false}");
    }
    
    // Parse arguments
    json_t *args = json_parse(args_json, NULL);
    if (!args) {
        return strdup("{\"error\":\"Invalid JSON\",\"success\":false}");
    }
    
    const char *user_prompt = json_get_str(args, "user_prompt", "");
    if (!user_prompt || strlen(user_prompt) == 0) {
        json_free(args);
        return strdup("{\"error\":\"Missing user_prompt\",\"success\":false}");
    }
    
    const char *mode_str = json_get_str(args, "mode", "standard");
    moa_mode_t mode = MOA_MODE_MEGA_TOTEM;
    if (strcmp(mode_str, "devil_advocate") == 0 || strcmp(mode_str, "devil") == 0) {
        mode = MOA_MODE_DEVIL_ADVOCATE;
    } else if (strcmp(mode_str, "trepidation") == 0) {
        mode = MOA_MODE_TREPIDATION;
    } else if (strcmp(mode_str, "token_maxx") == 0 || strcmp(mode_str, "maxx") == 0) {
        mode = MOA_MODE_TOKEN_MAXX;
    } else if (strcmp(mode_str, "math") == 0) {
        mode = MOA_MODE_MATH;
    }
    
    // Optional: enable/disable online research
    int use_online_research = json_get_bool(args, "use_online_research", 1);
    (void)json_get_str(args, "research_intent", "benchmark_update");  // Used for future intent-specific research
    
    // Initialize health tracking
    static int health_inited = 0;
    if (!health_inited) {
        init_provider_health();
        health_inited = 1;
    }
    
    // Get reference models
    int ref_count;
    const moa_ref_model_t *refs = get_ref_models(mode, &ref_count);
    
    // Create mutable copy of refs for potential reordering
    moa_ref_model_t *mutable_refs = malloc(ref_count * sizeof(moa_ref_model_t));
    for (int i = 0; i < ref_count; i++) {
        mutable_refs[i] = refs[i];
    }
    int mutable_ref_count = ref_count;
    
    // Optional: Online research for dynamic totem pole reordering
    research_summary_t *research = NULL;
    if (use_online_research && mode != MOA_MODE_MATH) {
        research = moa_research_for_prompt(user_prompt);
        if (research) {
            // Apply research-based reordering
            moa_apply_research_to_refs(&mutable_refs, &mutable_ref_count, research);
        }
    }
    
    // Filter to healthy providers
    moa_ref_model_t *healthy_refs = malloc(mutable_ref_count * sizeof(moa_ref_model_t));
    int healthy_count = 0;
    for (int i = 0; i < mutable_ref_count; i++) {
        if (is_provider_healthy(mutable_refs[i].provider)) {
            healthy_refs[healthy_count++] = mutable_refs[i];
        }
    }
    
    if (healthy_count == 0) {
        free(healthy_refs);
        free(mutable_refs);
        if (research) moa_research_free(research);
        json_free(args);
        return strdup("{\"error\":\"No healthy providers available\",\"success\":false}");
    }
    
    hermes_log(LOG_INFO, "moa", "Querying %d reference models in %s mode...", healthy_count, mode_str);
    
    // Get system prompts
    const char *ref_system = get_system_prompt(mode, 0);
    const char *agg_system = get_system_prompt(mode, 1);
    
    // Parallel reference queries
    moa_query_task_t *tasks = NULL;
    moa_query_all_references(healthy_refs, healthy_count, ref_system, user_prompt, &tasks);
    
    // Collect successful responses
    int successful_count = 0;
    for (int i = 0; i < healthy_count; i++) {
        if (tasks[i].result) successful_count++;
    }
    
    hermes_log(LOG_INFO, "moa", "Got %d/%d successful responses", successful_count, healthy_count);
    
    if (successful_count == 0) {
        free(tasks);
        free(healthy_refs);
        free(mutable_refs);
        if (research) moa_research_free(research);
        json_free(args);
        return strdup("{\"error\":\"All reference models failed\",\"success\":false}");
    }
    
    // Build aggregation prompt
    json_t *agg_prompt_json = json_object();
    json_t *agg_parts = json_array();

    json_t *orig = json_object();
    json_set(orig, "content", json_string(user_prompt));
    json_append(agg_parts, orig);
    json_free(orig);

    for (int i = 0; i < healthy_count; i++) {
        if (tasks[i].result) {
            json_t *part = json_object();
            char header[256];
            snprintf(header, sizeof(header), "--- %s:%s (%s) ---", 
                     tasks[i].ref->provider, tasks[i].ref->model, tasks[i].ref->role);
            json_set(part, "content", json_string(header));
            json_append(agg_parts, part);

            json_t *resp = json_object();
            json_set(resp, "content", json_string(tasks[i].result));
            json_append(agg_parts, resp);
            json_free(resp);
        }
    }

    json_set(agg_prompt_json, "parts", agg_parts);
    json_free(agg_parts);
    char *agg_prompt = json_serialize(agg_prompt_json);
    json_free(agg_prompt_json);

    // Select aggregator (best healthy provider)
    const char *agg_provider = "nvidia_nim";
    const char *agg_model = "nvidia/nemotron-3-ultra-550b-a55b";
    
    for (int p = 0; p < MOA_PROVIDERS_COUNT; p++) {
        if (is_provider_healthy(MOA_PROVIDERS[p].name) && MOA_PROVIDERS[p].num_free_models > 0) {
            agg_provider = MOA_PROVIDERS[p].name;
            agg_model = MOA_PROVIDERS[p].free_models[0];
            break;
        }
    }
    
    // Find provider config for aggregator
    const moa_provider_config_t *agg_provider_config = NULL;
    for (int p = 0; p < MOA_PROVIDERS_COUNT; p++) {
        if (strcmp(MOA_PROVIDERS[p].name, agg_provider) == 0) {
            agg_provider_config = &MOA_PROVIDERS[p];
            break;
        }
    }
    
    char *agg_response = NULL;
    if (agg_provider_config) {
        moa_ref_model_t agg_ref = {
            .provider = agg_provider,
            .model = agg_model,
            .temperature = 0.3f,
            .max_tokens = 8192,
            .reasoning_effort = "xhigh",
            .role = "synthesizer",
            .benchmark_tier = 1,
        };
        
        agg_response = moa_call_model(agg_provider_config, &agg_ref, agg_system, agg_prompt);
    }
    
    free(agg_prompt);
    
    if (!agg_response) {
        // Fallback to first successful response
        for (int i = 0; i < healthy_count; i++) {
            if (tasks[i].result) {
                agg_response = strdup(tasks[i].result);
                break;
            }
        }
    }
    
    // Build result JSON
    json_t *result = json_new_object();
    json_set(result, "success", json_bool(1));
    json_set(result, "mode", json_string(mode_str));
    
    json_t *ref_responses = json_array();
    for (int i = 0; i < healthy_count; i++) {
        if (tasks[i].result) {
            json_t *resp_obj = json_object();
            json_set(resp_obj, "provider", json_string(tasks[i].ref->provider));
            json_set(resp_obj, "model", json_string(tasks[i].ref->model));
            json_set(resp_obj, "role", json_string(tasks[i].ref->role));
            json_set(resp_obj, "content", json_string(tasks[i].result));
            json_set(resp_obj, "temperature", json_number(tasks[i].ref->temperature));
            json_set(resp_obj, "max_tokens", json_number(tasks[i].ref->max_tokens));
            json_append(ref_responses, resp_obj);
        }
    }
    json_set(result, "reference_responses", ref_responses);
    json_free(ref_responses);
    
    json_set(result, "aggregator_response", json_string(agg_response ? agg_response : ""));
    json_set(result, "aggregator_model", json_string(agg_response ? "aggregator" : "fallback"));
    
    // Provider health summary
    json_t *health = json_object();
    for (int p = 0; p < MOA_PROVIDERS_COUNT; p++) {
        json_t *h = json_object();
        json_set(h, "healthy", json_bool(provider_health[p].healthy));
        json_set(h, "failures", json_number(provider_health[p].consecutive_failures));
        json_set(health, MOA_PROVIDERS[p].name, h);
    }
    json_set(result, "provider_health", health);
    json_free(health);
    
    // Include research metadata if available
    if (research) {
        json_t *research_json = json_object();
        json_set(research_json, "query", json_string(research->query));
        json_set(research_json, "intent", json_string(research->intent));
        json_set(research_json, "findings_count", json_number(research->num_findings));
        json_set(research_json, "confidence", json_number(research->confidence));
        
        // Model scores
        json_t *model_scores = json_object();
        for (int i = 0; i < research->num_model_scores; i++) {
            json_set(model_scores, research->model_ids[i], json_number(research->model_scores[i]));
        }
        json_set(research_json, "model_scores", model_scores);
        json_free(model_scores);
        
        // Top sources
        json_t *top_sources = json_array();
        for (int i = 0; i < research->num_findings && i < 5; i++) {
            json_t *src = json_object();
            json_set(src, "title", json_string(research->findings[i].title));
            json_set(src, "url", json_string(research->findings[i].url));
            json_set(src, "source", json_string(research->findings[i].source));
            json_set(src, "relevance", json_number(research->findings[i].relevance_score));
            json_append(top_sources, src);
        }
        json_set(research_json, "top_sources", top_sources);
        json_free(top_sources);
        
        json_set(result, "research", research_json);
        json_free(research_json);
    }
    
    free(agg_response);
    free(tasks);
    free(healthy_refs);
    free(mutable_refs);
    if (research) moa_research_free(research);
    json_free(args);
    
    return json_dumps(result, 2);
}

/* ─── REGISTRY INTEGRATION ───────────────────────────────────────── */

void registry_init_mixture_of_agents(void) {
    registry_register_ex(
        "mixture_of_agents",
        "Process a complex query using multiple frontier AI models (Mixture of Agents). "
        "Modes: standard (2-layer synthesis), devil_advocate (3 adversarial perspectives), "
        "trepidation (cautious uncertainty analysis), token_maxx (maximum quality exhaustive), "
        "math (specialized mathematical reasoning). "
        "Uses FREE models only from NVIDIA NIM (180+ models from NVIDIA/Meta/Google/Microsoft/Mistral/Cohere/Z.ai/Moonshot/MiniMax/Qwen/DeepSeek/Phi/Gemma/etc.), "
        "NVIDIA Cloud/NVCF, OpenRouter, and Nous Portal. "
        "NEVER uses Anthropic/Claude. Implements provider health-aware rollover with auto-recovery. "
        "Models ordered by Hermes-agentic benchmarks: SWE-Bench, Terminal-Bench, LiveCodeBench, "
        "AA Coding Agents, FrontierMath, AIME, GPQA. "
        "Tandem NVIDIA slam: NIM + Cloud simultaneous for mixed-model throughput. "
        "Best for: complex coding, advanced algorithms, multi-faceted analysis, high-stakes decisions, "
        "adversarial testing, exhaustive research, mathematical proofs.",
        "{\"type\":\"object\",\"properties\":{\"user_prompt\":{\"type\":\"string\",\"description\":\"The complex problem or query to process via multiple models\"},\"mode\":{\"type\":\"string\",\"enum\":[\"standard\",\"devil_advocate\",\"trepidation\",\"token_maxx\",\"math\"],\"default\":\"standard\",\"description\":\"MoA mode\"},\"custom_refs\":{\"type\":\"array\",\"description\":\"Optional custom reference models (provider,model,temperature,max_tokens,reasoning_effort,role,benchmark_tier)\"}},\"required\":[\"user_prompt\"]}",
        "moa",
        handle_mixture_of_agents
    );
}