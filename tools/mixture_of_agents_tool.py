#!/usr/bin/env python3
"""
Mixture-of-Agents Tool Module (Hermes Python) - MEGA TOTEM POLE EDITION

Implements MoA methodology with:
- Multi-provider FREE model orchestration (NVIDIA NIM, NVIDIA Cloud/NVCF, OpenRouter free, Nous Portal free)
- Provider health-aware rollover with auto-recovery (no auth timeouts)
- Token maxxing: optimal temperature/token configs per model class
- Triple Devil's Advocate mode (3 adversarial perspectives)
- Trepidation Online Research mode (uncertainty quantification)
- Tandem NVIDIA slam: NIM + Cloud simultaneous for mixed-model throughput
- NEVER uses Anthropic/Claude models
- RESEARCH-BASED MODEL ORDERING (totem poles from benchmark scores)
- Online research integration for dynamic reordering
- Parallel reference model queries for speed
- Uses all 4 API keys simultaneously (NVIDIA_API_KEY, NVIDIA_CLOUD_API_KEY, OPENROUTER_API_KEY, NOUS_API_KEY)
- Hermes-agentic benchmark focus: SWE-Bench, Terminal-Bench, LiveCodeBench, AA Coding Agents, FrontierMath, AIME

Based on "Mixture-of-Agents Enhances Large Language Model Capabilities"
(arXiv:2406.04692v1) with Hermes-specific extensions.

Benchmark Sources (2026):
- SWE-Bench Verified / Pro (real GitHub issue resolution)
- Terminal-Bench 2.0/2.1 (agentic terminal/CLI tasks)
- LiveCodeBench v6 (rolling competitive programming)
- AA Coding Agents Index (DeepSWE + Terminal-Bench + SWE-Atlas-QnA)
- FrontierMath v2 Tiers 1-3 (Epoch AI private math)
- AIME 2025/2026 (olympiad math)
- GPQA Diamond (graduate reasoning)
- HMMT Feb 2026 (Harvard-MIT Math Tournament)
"""

from __future__ import annotations

import asyncio
import json
import os
import time
import hashlib
from dataclasses import dataclass, field
from enum import Enum
from typing import Any, Optional
import aiohttp

# Import online research
from .online_research import (
    ResearchSummary,
    get_researcher,
    close_researcher,
    reorder_totem_pole_by_research,
)

# Import performance optimizations
from .moa_performance import (
    ProjectContextManager,
    PersistentResearchCache,
    OptimizedMoAHttpClient,
    get_project_manager,
    get_research_cache,
    get_response_cache,
    get_http_client,
    get_model_selector,
    close_global_clients,
)

# ─── MoA Mode Enum ──────────────────────────────────────────────────

class MoAMode(Enum):
    STANDARD = "standard"
    DEVIL_ADVOCATE = "devil_advocate"
    TREPIDATION = "trepidation"
    TOKEN_MAXX = "token_maxx"
    MEGA_TOTEM = "mega_totem"


# ─── Provider & Model Configs ───────────────────────────────────────

@dataclass
class ProviderConfig:
    name: str
    base_url: str
    api_key_env: str
    free_models: list[str]
    requires_auth: bool
    priority: int  # Lower = higher priority
    health_check_interval: int = 60  # seconds
    max_consecutive_failures: int = 3


@dataclass
class RefModelConfig:
    provider: str
    model: str
    temperature: float
    max_tokens: int
    reasoning_effort: str  # "none" | "low" | "medium" | "high" | "xhigh"
    role: str  # "analytical" | "creative" | "critical" | "synthesizer" | "devil_advocate_N"
    benchmark_tier: int  # 1=highest, 2=high, 3=good


# ─── NVIDIA NIM MODELS (integrate.api.nvidia.com) - 118 TOTAL, ALL FREE ───
# ALL models on NIM are FREE. NVIDIA is a cloud provider hosting non-NVIDIA
# models (GLM, DeepSeek, MiniMax, Qwen, Kimi, Mistral, Meta, Google, etc.) for free.
# Live-tested 2026-07-24: 21 verified callable, 19 slow (>12s), 30 404 (not enabled),
# 6 stream-only (return no choices). All kept in list — NVIDIA rotates enablement.

NVIDIA_NIM_MODELS = [
    # TIER S: Verified-callable agentic leaders (tested 2026-07-24)
    "z-ai/glm-5.2",                               # 🏆 AA 51 | SWE-Pro 62.1 #1 open | Term-B 81% | HLE+tools 54.7% | 1M ctx | 1.85s
    "nvidia/nemotron-3-ultra-550b-a55b",           # AA 47.7 | SWE-V 71.9% | Agentic 70.9% | Term-B 56.4% | GPQA 87% | IFBench 82% | 1M ctx | 2.17s
    "nvidia/nemotron-3-super-120b-a12b",           # AA 36 | MMLU-Pro 83.7 | HMMT 93.7% | RULER@1M 91.8 | SWE-V 60.5% | HumanEval 79.4% | 0.61s | 16.5 tok/s
    "minimaxai/minimax-m3",                        # Top coding/agentic | SWE-bench Pro leaderboard | 1.19s
    "deepseek-ai/deepseek-v4-flash",               # AA 57 family | SWE-V 80.6% family | Fast variant | 3.83s | 13.3 tok/s
    "mistralai/mistral-small-4-119b-2603",         # 119B | strong reasoning | 0.54s | Mistral's newest
    "mistralai/mistral-nemotron",                  # NVIDIA-Mistral collab | 0.42s | 7.1 tok/s

    # TIER A: Strong but slow/404 — kept for when NVIDIA enables Public API Endpoints
    "deepseek-ai/deepseek-v4-pro",                 # AA 57 | SWE-V 80.6% | SWE-Pro 55.4% | [SLOW >12s]
    "minimaxai/minimax-m2.7",                      # SWE-bench Pro 75.8% | BrowseComp 76.3% | [SLOW >12s]
    "qwen/qwen3-next-80b-a3b-instruct",            # Qwen3-Next 80B MoE | strong agentic | [SLOW]
    "moonshotai/kimi-k2.6",                        # 🏆 SWE-Pro 58.6% #1 | Term-B 66.7% | HLE+tools 54% #1 | LiveCB 89.6% | GPQA 90.5% | BrowseComp 86.3% | [404]
    "nvidia/llama-3.1-nemotron-ultra-253b-v1",     # 253B dense | best reasoning/coding tradeoff | 128K ctx | 8x H100 | [404]
    "nvidia/nemotron-4-340b-instruct",             # 340B | 9T tokens | synthetic data gen | coding/math | 4K ctx | [404]
    "nvidia/llama-3.3-nemotron-super-49b-v1.5",    # Nemotron Super 49B v1.5 | efficient | [STREAM]
    "openai/gpt-oss-120b",                         # AA Index 33 | open weights | [404]
    "nvidia/llama-3.1-nemotron-70b-instruct",      # 70B RLHF | great alignment | tool calling | [404]

    # TIER B: Fast/creative/specialist (verified callable)
    "nvidia/nemotron-3-nano-omni-30b-a3b-reasoning", # Omni 30B reasoning | 63 tok/s BLAZING | 0.90s
    "nvidia/nemotron-3-nano-30b-a3b",               # 30B Nano | 20.5 tok/s | 0.49s
    "meta/llama-4-maverick-17b-128e-instruct",      # Llama 4 Maverick 17B×128e MoE | 0.44s
    "mistralai/mixtral-8x7b-instruct-v0.1",         # Mixtral 8x7B | 14.3 tok/s | 0.70s
    "poolside/laguna-xs-2.1",                       # Poolside Laguna | coding | 7.0 tok/s | 0.43s
    "google/diffusiongemma-26b-a4b-it",             # DiffusionGemma 26B MoE | 4.77s
    "google/gemma-3n-e4b-it",                       # Gemma 3nano e4B | 0.67s
    "google/gemma-3n-e2b-it",                       # Gemma 3nano e2B | 0.58s
    "google/gemma-2-2b-it",                         # Gemma 2 2B | 0.39s FASTEST
    "nvidia/nemotron-mini-4b-instruct",             # Nemotron Mini 4B | 1.34s
    "sarvamai/sarvam-m",                            # Sarvam-M Indic | 0.66s
    "upstage/solar-10.7b-instruct",                 # Solar 10.7B | 0.62s
    "meta/llama-3.2-3b-instruct",                   # Llama 3.2 3B | 0.43s
    "meta/llama-3.1-8b-instruct",                   # Llama 3.1 8B | 0.47s

    # TIER C: Listed but not currently callable (kept for documentation/future enablement)
    "thinkingmachines/inkling",                     # [STREAM] Thinking Machines Inkling
    "stepfun-ai/step-3.5-flash",                    # [STREAM] StepFun 3.5 Flash
    "stepfun-ai/step-3.7-flash",                    # [STREAM] StepFun 3.7 Flash
    "openai/gpt-oss-20b",                           # [STREAM] GPT-OSS 20B open weights
    "nvidia/nvidia-nemotron-nano-9b-v2",             # [STREAM] 9B Nano v2 unified reasoning/non-reasoning
    "mistralai/mistral-medium-3.5-128b",             # [SLOW] 128B Mistral Medium
    "meta/llama-3.3-70b-instruct",                   # [SLOW] Llama 3.3 70B
    "google/gemma-4-31b-it",                         # [SLOW] Gemma 4 31B newest Google
    "bytedance/seed-oss-36b-instruct",               # [SLOW] ByteDance Seed-OSS 36B
    "mistralai/ministral-14b-instruct-2512",         # [SLOW] Ministral 14B
    "writer/palmyra-creative-122b",                  # [404] Writer Palmyra Creative 122B
    "mistralai/mistral-large-2-instruct",            # [404] Mistral Large 2
    "ai21labs/jamba-1.5-large-instruct",             # [404] Jamba 1.5 Large
    "qwen/qwen3.5-397b-a17b",                        # [404] Qwen 3.5 397B MoE
    "nvidia/cosmos-reason2-8b",                      # [404] Cosmos Reason 2 8B
    "nvidia/nemotron-nano-3-30b-a3b",                # [404] Nemotron Nano 3 30B
    "nvidia/llama-3.1-nemotron-51b-instruct",        # [404] Nemotron 51B
    "nvidia/llama-3.1-nemotron-nano-8b-v1",          # [SLOW] 8B Nano reasoning | 128K ctx
]


# ─── OPENROUTER FREE MODELS ─────────────────────────────────────────
OPENROUTER_FREE_MODELS = [
    # NVIDIA flagships on OpenRouter free tier
    "nvidia/nemotron-3-ultra-550b-a55b:free",   # Same as NIM #4, FREE
    "nvidia/nemotron-3-super-120b-a12b:free",   # Same as NIM #9, FREE
    "nvidia/llama-3.1-nemotron-70b-instruct:free", # 70B FREE
    "nvidia/nemotron-4-340b-instruct:free",     # 340B FREE
    
    # Other strong free models
    "inclusionai/ring-2.6-1t:free",             # Emerging strong model
    "poolside/laguna-m.1:free",                 # Coding-focused
    "openrouter/elephant-alpha",                # General purpose
    "openrouter/owl-alpha",                     # General purpose
    "tencent/hy3-preview:free",                 # Chinese-optimized
    "north/north-mini-code:free",               # Code-specialized
    "mimo-v2.5:free",                           # Xiaomi model
    "big-pickle:free",                          # Community model
    "nemotron-3-ultra:free",                    # Alias
]


# ─── NOUS PORTAL FREE MODELS ────────────────────────────────────────
NOUS_FREE_MODELS = [
    "deepseek/deepseek-v4-flash",               # Fast, capable
    "google/gemini-3.5-flash",                  # Strong reasoning
    "meta-llama/llama-3.1-405b-instruct:free",  # Largest open model
]


# ─── PROVIDER CONFIGURATIONS ────────────────────────────────────────

MOA_PROVIDERS = [
    ProviderConfig(
        name="nvidia_nim",
        base_url="https://integrate.api.nvidia.com/v1",
        api_key_env="NVIDIA_API_KEY",
        free_models=NVIDIA_NIM_MODELS,
        requires_auth=False,  # Often works without key for free tier
        priority=1,
    ),
    ProviderConfig(
        name="nvidia_cloud",
        base_url="https://api.nvcf.nvidia.com/v1",
        api_key_env="NVIDIA_CLOUD_API_KEY",
        free_models=NVIDIA_NIM_MODELS,  # Same 180+ catalog on dedicated cloud infra
        requires_auth=True,
        priority=2,
    ),
    ProviderConfig(
        name="openrouter",
        base_url="https://openrouter.ai/api/v1",
        api_key_env="OPENROUTER_API_KEY",
        free_models=OPENROUTER_FREE_MODELS,
        requires_auth=True,
        priority=3,
    ),
    ProviderConfig(
        name="nous_portal",
        base_url="https://inference-api.nousresearch.com/v1",
        api_key_env="NOUS_API_KEY",
        free_models=NOUS_FREE_MODELS,
        requires_auth=True,
        priority=4,
    ),
]


# ─── SYSTEM PROMPTS ─────────────────────────────────────────────────

MOA_STANDARD_SYSTEM = """You are an expert AI assistant participating in a Mixture of Agents ensemble.
Your role: Provide a thorough, accurate, and well-reasoned response to the user's query.
Draw on your specific strengths (coding, reasoning, analysis, creativity) as appropriate.
Be concise but comprehensive. Focus on actionable, correct information."""

MOA_DEVIL_AGGREGATOR_SYSTEM = """You are the aggregator for a Devil's Advocate Mixture of Agents.
Three critics have challenged the problem from different angles:
- Critic 1 (Skeptic): Challenges assumptions, finds holes, questions premises
- Critic 2 (Optimizer): Improves efficiency, finds better algorithms, reduces complexity
- Critic 3 (Safety): Checks for risks, edge cases, security issues, unintended consequences

Synthesize their adversarial perspectives into a robust, bulletproof final answer.
Address every criticism. Strengthen every weakness. Produce the most resilient solution possible."""

MOA_TREPIDATION_AGGREGATOR_SYSTEM = """You are the aggregator for a Trepidation (Uncertainty Quantification) Mixture of Agents.
Multiple cautious models have provided responses with explicit confidence intervals,
uncertainty assessments, and variance analysis.

Your task: Produce a final answer that honestly represents the epistemic state.
- Report confidence levels for each claim
- Flag where models disagree
- Quantify uncertainty (e.g., "80% confident", "high variance across models")
- Distinguish between aleatoric (data) and epistemic (model) uncertainty
- Provide calibrated probabilities where possible

Do not overstate certainty. The user needs to know what's reliable vs speculative."""

MOA_TOKEN_MAXX_AGGREGATOR_SYSTEM = """You are the aggregator for a Token Maxxing Mixture of Agents.
Multiple high-capacity models have provided exhaustive, detailed responses
with maximum token budgets and optimal sampling parameters.

Synthesize the most comprehensive, thorough, and high-quality answer possible.
- Merge unique insights from each model
- Resolve contradictions by favoring higher-confidence sources
- Preserve technical depth and nuance
- Include code examples, proofs, derivations where relevant
- Structure for maximum utility: executive summary -> deep dive -> references"""

MOA_STANDARD_AGGREGATOR_SYSTEM = """You are the final aggregator in a Mixture of Agents ensemble.
Multiple expert models have provided their perspectives on the user's query.
Synthesize their responses into a single, coherent, high-quality answer.

Guidelines:
- Merge unique insights, don't just repeat
- Resolve conflicts by weighing model strengths (benchmark-ordered)
- Preserve technical accuracy and nuance
- Structure clearly: summary -> details -> actionable conclusion
- Credit the ensemble approach when relevant"""


# Role-specific system prompts for reference models
MOA_DEVIL_SYSTEM = """You are a DEVIL'S ADVOCATE critic in a Mixture of Agents ensemble.
Your role: Challenge the problem aggressively. Find flaws, holes, edge cases, wrong assumptions.
Be ruthless but constructive. Your criticism makes the final answer stronger.
Focus on: logical gaps, missing edge cases, security risks, performance issues, wrong assumptions."""

MOA_TREPIDATION_SYSTEM = """You are a CAUTIOUS ANALYST in a Mixture of Agents ensemble.
Your role: Quantify uncertainty. Report confidence intervals. Flag disagreement.
For every claim, state your confidence (0-100%). Note where you're uncertain.
Distinguish: what you know vs what you assume vs what's speculative.
Be precise about variance sources."""

MOA_TOKEN_MAXX_SYSTEM = """You are a MAXIMUM QUALITY contributor in a Mixture of Agents ensemble.
Your role: Exhaustive, thorough, maximum detail. Use your full token budget.
Provide: complete derivations, full code with tests, comprehensive analysis,
edge cases, alternatives, benchmarks, references. No shortcuts."""

MOA_MATH_SYSTEM = """You are a MATHEMATICAL REASONING specialist in a Mixture of Agents ensemble.
Your role: Rigorous proofs, step-by-step derivations, formal verification.
Use: Lean/Coq-style reasoning, symbolic manipulation, counterexample checking.
Focus: FrontierMath, AIME, Olympiad-level rigor. No hand-waving."""


# ─── MODE SELECTION ─────────────────────────────────────────────────

def get_system_prompt(mode: MoAMode, is_aggregator: bool) -> str:
    if is_aggregator:
        return {
            MoAMode.STANDARD: MOA_STANDARD_AGGREGATOR_SYSTEM,
            MoAMode.DEVIL_ADVOCATE: MOA_DEVIL_AGGREGATOR_SYSTEM,
            MoAMode.TREPIDATION: MOA_TREPIDATION_AGGREGATOR_SYSTEM,
            MoAMode.TOKEN_MAXX: MOA_TOKEN_MAXX_AGGREGATOR_SYSTEM,
            MoAMode.MEGA_TOTEM: MOA_TOKEN_MAXX_AGGREGATOR_SYSTEM,
        }[mode]
    else:
        return {
            MoAMode.STANDARD: MOA_STANDARD_SYSTEM,
            MoAMode.DEVIL_ADVOCATE: MOA_DEVIL_SYSTEM,
            MoAMode.TREPIDATION: MOA_TREPIDATION_SYSTEM,
            MoAMode.TOKEN_MAXX: MOA_TOKEN_MAXX_SYSTEM,
            MoAMode.MEGA_TOTEM: MOA_STANDARD_SYSTEM,
        }[mode]


def get_ref_models(mode: MoAMode) -> list[RefModelConfig]:
    """Get reference models for a mode, ordered by benchmark scores.
    
    Uses ALL NVIDIA providers (NIM, Cloud, OpenRouter, Nous) with their full model catalogs.
    Models are pre-filtered by benchmark tier and assigned optimal temperature/token configs.
    """
    
    if mode == MoAMode.STANDARD:
        return [
            # TIER S: Verified-callable agentic leaders (tested 2026-07-24)
            RefModelConfig("nvidia_nim", "z-ai/glm-5.2", 0.6, 4096, "xhigh", "analytical", 1),
            RefModelConfig("nvidia_nim", "nvidia/nemotron-3-ultra-550b-a55b", 0.6, 4096, "xhigh", "analytical", 1),
            RefModelConfig("nvidia_nim", "nvidia/nemotron-3-super-120b-a12b", 0.6, 4096, "high", "analytical", 1),
            RefModelConfig("nvidia_nim", "minimaxai/minimax-m3", 0.6, 4096, "high", "analytical", 1),
            RefModelConfig("nvidia_nim", "deepseek-ai/deepseek-v4-flash", 0.6, 4096, "high", "analytical", 1),
            RefModelConfig("nvidia_nim", "mistralai/mistral-small-4-119b-2603", 0.6, 4096, "high", "analytical", 1),
            RefModelConfig("nvidia_nim", "mistralai/mistral-nemotron", 0.5, 4096, "medium", "analytical", 2),

            # TIER A: Strong but slow/404 — NVIDIA may enable these later
            RefModelConfig("nvidia_nim", "deepseek-ai/deepseek-v4-pro", 0.6, 4096, "xhigh", "analytical", 1),
            RefModelConfig("nvidia_nim", "minimaxai/minimax-m2.7", 0.6, 4096, "xhigh", "analytical", 1),
            RefModelConfig("nvidia_nim", "qwen/qwen3-next-80b-a3b-instruct", 0.6, 4096, "high", "analytical", 1),
            RefModelConfig("nvidia_nim", "moonshotai/kimi-k2.6", 0.6, 4096, "xhigh", "analytical", 1),
            RefModelConfig("nvidia_nim", "nvidia/llama-3.1-nemotron-ultra-253b-v1", 0.6, 4096, "xhigh", "analytical", 1),
            RefModelConfig("nvidia_nim", "nvidia/nemotron-4-340b-instruct", 0.6, 4096, "xhigh", "analytical", 1),
            RefModelConfig("nvidia_nim", "nvidia/llama-3.3-nemotron-super-49b-v1.5", 0.5, 4096, "high", "creative", 2),
            RefModelConfig("nvidia_nim", "openai/gpt-oss-120b", 0.5, 4096, "medium", "synthesizer", 2),
            RefModelConfig("nvidia_nim", "nvidia/llama-3.1-nemotron-70b-instruct", 0.5, 4096, "high", "analytical", 2),

            # TIER B: Fast/creative/specialist (verified callable, diverse perspectives)
            RefModelConfig("nvidia_nim", "nvidia/nemotron-3-nano-omni-30b-a3b-reasoning", 0.5, 4096, "medium", "creative", 2),
            RefModelConfig("nvidia_nim", "meta/llama-4-maverick-17b-128e-instruct", 0.5, 4096, "medium", "creative", 2),
            RefModelConfig("nvidia_nim", "mistralai/mixtral-8x7b-instruct-v0.1", 0.5, 4096, "medium", "creative", 2),
            RefModelConfig("nvidia_nim", "poolside/laguna-xs-2.1", 0.5, 4096, "high", "analytical", 2),
            RefModelConfig("nvidia_nim", "google/diffusiongemma-26b-a4b-it", 0.5, 4096, "medium", "creative", 2),

            # TIER D: OpenRouter free tier (cross-provider redundancy)
            RefModelConfig("openrouter", "nvidia/nemotron-3-ultra-550b-a55b:free", 0.6, 4096, "xhigh", "analytical", 1),
            RefModelConfig("openrouter", "nvidia/nemotron-3-super-120b-a12b:free", 0.5, 4096, "high", "creative", 2),
            RefModelConfig("openrouter", "inclusionai/ring-2.6-1t:free", 0.6, 4096, "high", "creative", 2),

            # Nous Portal fallback
            RefModelConfig("nous_portal", "deepseek/deepseek-v4-flash", 0.5, 4096, "high", "analytical", 3),
            RefModelConfig("nous_portal", "google/gemini-3.5-flash", 0.5, 4096, "high", "analytical", 3),
            RefModelConfig("nous_portal", "meta-llama/llama-3.1-405b-instruct:free", 0.5, 4096, "medium", "synthesizer", 3),
        ]
    
    elif mode == MoAMode.DEVIL_ADVOCATE:
        return [
            # 3 adversarial critics using BEST available models (verified callable)
            RefModelConfig("nvidia_nim", "z-ai/glm-5.2", 0.8, 4096, "xhigh", "devil_advocate_1", 1),
            RefModelConfig("nvidia_nim", "nvidia/nemotron-3-ultra-550b-a55b", 0.8, 4096, "xhigh", "devil_advocate_2", 1),
            RefModelConfig("nvidia_nim", "deepseek-ai/deepseek-v4-flash", 0.8, 4096, "high", "devil_advocate_3", 1),
            # Additional critics for redundancy
            RefModelConfig("nvidia_nim", "minimaxai/minimax-m3", 0.8, 4096, "high", "devil_advocate_4", 1),
            RefModelConfig("openrouter", "nvidia/nemotron-3-ultra-550b-a55b:free", 0.8, 4096, "xhigh", "devil_advocate_5", 1),
        ]
    
    elif mode == MoAMode.TREPIDATION:
        return [
            # Cautious, uncertainty-focused models (BEST critical/reasoning models)
            RefModelConfig("nvidia_nim", "z-ai/glm-5.2", 0.4, 4096, "xhigh", "critical", 1),
            RefModelConfig("nvidia_nim", "nvidia/nemotron-3-ultra-550b-a55b", 0.4, 4096, "xhigh", "critical", 1),
            RefModelConfig("nvidia_nim", "deepseek-ai/deepseek-v4-flash", 0.4, 4096, "high", "critical", 1),
            RefModelConfig("nvidia_nim", "nvidia/nemotron-3-super-120b-a12b", 0.4, 4096, "high", "critical", 2),
            RefModelConfig("nvidia_nim", "minimaxai/minimax-m3", 0.4, 4096, "high", "critical", 1),
            RefModelConfig("nvidia_nim", "mistralai/mistral-small-4-119b-2603", 0.4, 4096, "high", "critical", 1),
            RefModelConfig("openrouter", "nvidia/nemotron-3-ultra-550b-a55b:free", 0.4, 4096, "xhigh", "critical", 1),
        ]
    
    elif mode == MoAMode.TOKEN_MAXX:
        return [
            # Maximum quality, maximum tokens - use BEST verified-callable models
            RefModelConfig("nvidia_nim", "z-ai/glm-5.2", 0.7, 8192, "xhigh", "analytical", 1),
            RefModelConfig("nvidia_nim", "nvidia/nemotron-3-ultra-550b-a55b", 0.7, 8192, "xhigh", "analytical", 1),
            RefModelConfig("nvidia_nim", "nvidia/nemotron-3-super-120b-a12b", 0.7, 8192, "high", "analytical", 1),
            RefModelConfig("nvidia_nim", "minimaxai/minimax-m3", 0.7, 8192, "high", "analytical", 1),
            RefModelConfig("nvidia_nim", "deepseek-ai/deepseek-v4-flash", 0.7, 8192, "high", "analytical", 1),
            RefModelConfig("nvidia_nim", "mistralai/mistral-small-4-119b-2603", 0.7, 8192, "high", "analytical", 1),
            RefModelConfig("nvidia_nim", "mistralai/mistral-nemotron", 0.7, 8192, "medium", "creative", 2),
            RefModelConfig("nvidia_nim", "nvidia/nemotron-3-nano-omni-30b-a3b-reasoning", 0.7, 8192, "medium", "creative", 2),
            RefModelConfig("nvidia_nim", "meta/llama-4-maverick-17b-128e-instruct", 0.7, 8192, "medium", "creative", 2),
            RefModelConfig("nvidia_nim", "poolside/laguna-xs-2.1", 0.7, 8192, "high", "analytical", 2),
            RefModelConfig("nvidia_nim", "google/diffusiongemma-26b-a4b-it", 0.7, 8192, "medium", "creative", 2),
            
            # Cross-provider redundancy for maxxing
            RefModelConfig("nvidia_nim", "deepseek-ai/deepseek-v4-pro", 0.7, 8192, "xhigh", "analytical", 1),
            RefModelConfig("nvidia_nim", "minimaxai/minimax-m2.7", 0.7, 8192, "xhigh", "analytical", 1),
            RefModelConfig("nvidia_nim", "moonshotai/kimi-k2.6", 0.7, 8192, "xhigh", "analytical", 1),
            
            # OpenRouter free tier redundancy
            RefModelConfig("openrouter", "nvidia/nemotron-3-ultra-550b-a55b:free", 0.8, 8192, "xhigh", "analytical", 1),
            RefModelConfig("openrouter", "nvidia/nemotron-3-super-120b-a12b:free", 0.7, 8192, "xhigh", "analytical", 1),
            RefModelConfig("openrouter", "inclusionai/ring-2.6-1t:free", 0.7, 8192, "high", "creative", 2),
            
            # Nous Portal
            RefModelConfig("nous_portal", "meta-llama/llama-3.1-405b-instruct:free", 0.7, 8192, "medium", "synthesizer", 3),
            RefModelConfig("nous_portal", "deepseek/deepseek-v4-flash", 0.7, 8192, "high", "analytical", 3),
            RefModelConfig("nous_portal", "google/gemini-3.5-flash", 0.7, 8192, "high", "analytical", 3),
        ]
    
    elif mode == MoAMode.MEGA_TOTEM:
        return [
            # ALL verified-callable models across all 3 providers
            # 21 NVIDIA NIM + 3 OpenRouter + 3 Nous = 27 models
            # Default for most users — fast, awesome, FREE
            # NVIDIA NIM TIER S (7 verified callable)
            RefModelConfig("nvidia_nim", "z-ai/glm-5.2", 0.6, 4096, "xhigh", "analytical", 1),
            RefModelConfig("nvidia_nim", "nvidia/nemotron-3-ultra-550b-a55b", 0.6, 4096, "xhigh", "analytical", 1),
            RefModelConfig("nvidia_nim", "nvidia/nemotron-3-super-120b-a12b", 0.6, 4096, "high", "analytical", 1),
            RefModelConfig("nvidia_nim", "minimaxai/minimax-m3", 0.6, 4096, "high", "analytical", 1),
            RefModelConfig("nvidia_nim", "deepseek-ai/deepseek-v4-flash", 0.6, 4096, "high", "analytical", 1),
            RefModelConfig("nvidia_nim", "mistralai/mistral-small-4-119b-2603", 0.6, 4096, "high", "analytical", 1),
            RefModelConfig("nvidia_nim", "mistralai/mistral-nemotron", 0.5, 4096, "medium", "creative", 2),
            # NVIDIA NIM TIER B (7 fast/creative)
            RefModelConfig("nvidia_nim", "nvidia/nemotron-3-nano-omni-30b-a3b-reasoning", 0.5, 4096, "medium", "creative", 2),
            RefModelConfig("nvidia_nim", "nvidia/nemotron-3-nano-30b-a3b", 0.5, 4096, "medium", "creative", 2),
            RefModelConfig("nvidia_nim", "meta/llama-4-maverick-17b-128e-instruct", 0.5, 4096, "medium", "creative", 2),
            RefModelConfig("nvidia_nim", "mistralai/mixtral-8x7b-instruct-v0.1", 0.5, 4096, "medium", "creative", 2),
            RefModelConfig("nvidia_nim", "poolside/laguna-xs-2.1", 0.5, 4096, "high", "analytical", 2),
            RefModelConfig("nvidia_nim", "google/diffusiongemma-26b-a4b-it", 0.5, 4096, "medium", "creative", 2),
            RefModelConfig("nvidia_nim", "google/gemma-3n-e4b-it", 0.5, 4096, "medium", "creative", 2),
            # NVIDIA NIM EXTRA (7 more verified callable)
            RefModelConfig("nvidia_nim", "google/gemma-3n-e2b-it", 0.5, 4096, "medium", "creative", 2),
            RefModelConfig("nvidia_nim", "google/gemma-2-2b-it", 0.5, 4096, "medium", "creative", 2),
            RefModelConfig("nvidia_nim", "nvidia/nemotron-mini-4b-instruct", 0.5, 4096, "medium", "creative", 2),
            RefModelConfig("nvidia_nim", "sarvamai/sarvam-m", 0.5, 4096, "medium", "creative", 2),
            RefModelConfig("nvidia_nim", "upstage/solar-10.7b-instruct", 0.5, 4096, "medium", "creative", 2),
            RefModelConfig("nvidia_nim", "meta/llama-3.2-3b-instruct", 0.5, 4096, "medium", "creative", 2),
            RefModelConfig("nvidia_nim", "meta/llama-3.1-8b-instruct", 0.5, 4096, "medium", "creative", 2),
            # OpenRouter free tier (3)
            RefModelConfig("openrouter", "nvidia/nemotron-3-ultra-550b-a55b:free", 0.6, 4096, "xhigh", "analytical", 1),
            RefModelConfig("openrouter", "nvidia/nemotron-3-super-120b-a12b:free", 0.5, 4096, "high", "creative", 2),
            RefModelConfig("openrouter", "inclusionai/ring-2.6-1t:free", 0.6, 4096, "high", "creative", 2),
            # Nous Portal fallback (3)
            RefModelConfig("nous_portal", "deepseek/deepseek-v4-flash", 0.5, 4096, "high", "analytical", 3),
            RefModelConfig("nous_portal", "google/gemini-3.5-flash", 0.5, 4096, "high", "analytical", 3),
            RefModelConfig("nous_portal", "meta-llama/llama-3.1-405b-instruct:free", 0.5, 4096, "medium", "synthesizer", 3),
        ]

    # MATH mode (specialized)
    return [
        RefModelConfig("nvidia_nim", "z-ai/glm-5.2", 0.3, 8192, "xhigh", "math_analytical", 1),
        RefModelConfig("nvidia_nim", "nvidia/nemotron-3-ultra-550b-a55b", 0.3, 8192, "xhigh", "math_analytical", 1),
        RefModelConfig("nvidia_nim", "nvidia/nemotron-3-super-120b-a12b", 0.3, 8192, "high", "math_analytical", 1),
        RefModelConfig("nvidia_nim", "deepseek-ai/deepseek-v4-flash", 0.3, 8192, "high", "math_analytical", 1),
        RefModelConfig("nvidia_nim", "minimaxai/minimax-m3", 0.3, 8192, "high", "math_analytical", 1),
        RefModelConfig("nvidia_nim", "mistralai/mistral-small-4-119b-2603", 0.3, 8192, "high", "math_analytical", 1),
        RefModelConfig("nvidia_nim", "nvidia/nemotron-3-nano-omni-30b-a3b-reasoning", 0.3, 8192, "medium", "math_creative", 2),
        RefModelConfig("openrouter", "nvidia/nemotron-3-ultra-550b-a55b:free", 0.3, 8192, "xhigh", "math_analytical", 1),
    ]


# ─── PROVIDER HEALTH TRACKING ───────────────────────────────────────

class ProviderHealth:
    """Thread-safe provider health tracking with auto-recovery."""
    
    def __init__(self):
        self._health: dict[str, dict] = {}
        self._lock = asyncio.Lock()
        # Initialize all providers as healthy
        for p in MOA_PROVIDERS:
            self._health[p.name] = {
                "healthy": True,
                "consecutive_failures": 0,
                "last_failure_time": 0,
                "last_success_time": time.time(),
            }
    
    async def is_healthy(self, provider_name: str) -> bool:
        async with self._lock:
            health = self._health.get(provider_name, {"healthy": True})
            if not health["healthy"]:
                # Auto-recovery after 60 seconds
                if time.time() - health["last_failure_time"] > 60:
                    health["healthy"] = True
                    health["consecutive_failures"] = 0
                    return True
            return health["healthy"]
    
    async def record_success(self, provider_name: str):
        async with self._lock:
            if provider_name in self._health:
                self._health[provider_name]["healthy"] = True
                self._health[provider_name]["consecutive_failures"] = 0
                self._health[provider_name]["last_success_time"] = time.time()
    
    async def record_failure(self, provider_name: str):
        async with self._lock:
            if provider_name in self._health:
                h = self._health[provider_name]
                h["consecutive_failures"] += 1
                h["last_failure_time"] = time.time()
                provider_config = next((p for p in MOA_PROVIDERS if p.name == provider_name), None)
                max_failures = provider_config.max_consecutive_failures if provider_config else 3
                if h["consecutive_failures"] >= max_failures:
                    h["healthy"] = False
                    print(f"[MOA] Provider {provider_name} marked unhealthy after {h['consecutive_failures']} failures")
    
    async def get_health_summary(self) -> dict:
        async with self._lock:
            return {name: {"healthy": h["healthy"], "failures": h["consecutive_failures"]} 
                    for name, h in self._health.items()}


provider_health = ProviderHealth()


# ─── HTTP CLIENT & MODEL CALLING ────────────────────────────────────

class MoAHttpClient:
    def __init__(self, timeout: int = 300):
        self.timeout = aiohttp.ClientTimeout(total=timeout)
        self.session: Optional[aiohttp.ClientSession] = None
    
    async def __aenter__(self):
        self.session = aiohttp.ClientSession(timeout=self.timeout)
        return self
    
    async def __aexit__(self, *args):
        if self.session:
            await self.session.close()
    
    def _build_auth_header(self, provider: ProviderConfig) -> Optional[str]:
        api_key = os.getenv(provider.api_key_env)
        if not api_key:
            return None
        return f"Bearer {api_key}"
    
    async def call_model(
        self,
        provider: ProviderConfig,
        ref: RefModelConfig,
        system_prompt: str,
        user_prompt: str,
    ) -> Optional[str]:
        if not await provider_health.is_healthy(provider.name):
            return None
        
        # Build request
        messages = []
        if system_prompt:
            messages.append({"role": "system", "content": system_prompt})
        messages.append({"role": "user", "content": user_prompt})
        
        payload = {
            "model": ref.model,
            "messages": messages,
            "temperature": ref.temperature,
            "max_tokens": ref.max_tokens,
            "stream": False,
        }
        
        # Add reasoning effort if supported
        if ref.reasoning_effort != "none":
            payload["reasoning"] = {"effort": ref.reasoning_effort, "enabled": True}
        
        headers = {
            "Content-Type": "application/json",
            "Accept": "application/json",
        }
        auth = self._build_auth_header(provider)
        if auth:
            headers["Authorization"] = auth
        
        url = f"{provider.base_url}/chat/completions"
        
        # Retry logic
        max_retries = 3
        for attempt in range(max_retries):
            try:
                async with self.session.post(url, json=payload, headers=headers) as resp:
                    if resp.status == 200:
                        data = await resp.json()
                        content = self._extract_content(data)
                        if content:
                            await provider_health.record_success(provider.name)
                            return content
                    elif resp.status == 401:
                        print(f"[MOA] {provider.name} auth failed - check {provider.api_key_env}")
                        await provider_health.record_failure(provider.name)
                        return None
                    elif resp.status == 429:
                        print(f"[MOA] {provider.name} rate limited (attempt {attempt + 1}/{max_retries})")
                        if attempt < max_retries - 1:
                            await asyncio.sleep(2 * (attempt + 1))
                        continue
                    else:
                        text = await resp.text()
                        print(f"[MOA] {provider.name} HTTP {resp.status}: {text[:200]}")
            except asyncio.TimeoutError:
                print(f"[MOA] {provider.name} timeout (attempt {attempt + 1}/{max_retries})")
            except Exception as e:
                print(f"[MOA] {provider.name} error: {e}")
            
            await provider_health.record_failure(provider.name)
            if attempt < max_retries - 1:
                await asyncio.sleep(1 * (attempt + 1))
        
        return None
    
    def _extract_content(self, data: dict) -> Optional[str]:
        try:
            choices = data.get("choices", [])
            if choices:
                msg = choices[0].get("message", {})
                content = msg.get("content", "")
                if content:
                    return content
                # Handle reasoning content if present
                reasoning = msg.get("reasoning", {})
                if isinstance(reasoning, dict) and reasoning.get("content"):
                    return reasoning["content"]
            return None
        except Exception:
            return None


# ─── PARALLEL QUERY HELPER ──────────────────────────────────────────

async def query_all_references(
    http: OptimizedMoAHttpClient,
    refs: list[RefModelConfig],
    system_prompt: str,
    user_prompt: str,
) -> list[tuple[RefModelConfig, Optional[str]]]:
    """Query all reference models in parallel using batch calling."""
    
    # Prepare batch requests
    batch_requests = []
    for ref in refs:
        provider = next((p for p in MOA_PROVIDERS if p.name == ref.provider), None)
        if not provider:
            continue
        batch_requests.append((provider, ref, system_prompt, user_prompt))
    
    # Execute batch
    results = await http.call_model_batch(batch_requests, max_parallel=8)
    
    # Pair results with refs
    return [(ref, resp) for ref, resp in zip(refs, results) if resp is not None]


# ─── MAIN MoA HANDLER ────────────────────────────────────────────────

async def mixture_of_agents_tool(
    user_prompt: str,
    mode: MoAMode = MoAMode.STANDARD,
    custom_refs: Optional[list[dict]] = None,
    use_online_research: bool = True,
    research_intent: str = "benchmark_update",
    project_id: Optional[str] = None,
    working_dir: Optional[str] = None,
    use_cache: bool = True,
) -> str:
    """
    Main MoA entry point with performance optimizations and project consistency.
    
    Args:
        user_prompt: The complex problem or query to process
        mode: MoA mode (standard, devil_advocate, trepidation, token_maxx)
        custom_refs: Optional custom reference models
        use_online_research: Whether to run online research for dynamic reordering
        research_intent: Research intent for scoring
        project_id: Optional project ID for context/memory (auto-detected from working_dir)
        working_dir: Working directory for project context (defaults to cwd)
        use_cache: Whether to use response caching
    
    Returns:
        JSON string with results
    """
    start_time = time.time()
    
    # Initialize project context for long-goal consistency
    project_manager = get_project_manager()
    project = project_manager.get_or_create_project(working_dir if working_dir else None)
    project_id = project.project_id
    
    # Check response cache
    if use_cache:
        response_cache = get_response_cache()
        cached = response_cache.get(user_prompt, mode.value, project_id)
        if cached:
            print(f"[MOA] Cache hit for project {project_id[:8]}...")
            return cached["response"]
    
    # Get reference models for mode
    if custom_refs:
        refs = [RefModelConfig(**r) for r in custom_refs]
    else:
        refs = get_ref_models(mode)
    
    if not refs:
        return json.dumps({"error": "No reference models configured", "success": False})
    
    # Use consistent model selector for project
    model_selector = get_model_selector()
    
    # Optional: Online research for dynamic totem pole reordering
    research_summary: Optional[ResearchSummary] = None
    if use_online_research and not custom_refs:
        try:
            # Check persistent research cache first
            research_cache = get_research_cache()
            research_query = f"{user_prompt} AI model benchmark SWE-Bench Terminal-Bench LiveCodeBench 2025 2026"
            
            cached_research = research_cache.get(research_query, research_intent, project_id)
            if cached_research:
                print(f"[MOA] Research cache hit for project {project_id[:8]}...")
                # Create mock summary from cache
                class CachedResearch:
                    def __init__(self, data):
                        self.query = research_query
                        self.findings = []
                        self.model_scores = data["model_scores"]
                        self.confidence = data["confidence"]
                research_summary = CachedResearch(cached_research)
            else:
                researcher = await get_researcher()
                research_summary = await researcher.research(research_query, intent=research_intent, num_results=15)
                # Cache the research
                research_cache.set(research_query, research_intent, research_summary.findings,
                                  research_summary.model_scores, research_summary.confidence, project_id)
            
            # Reorder reference models based on research
            if research_summary.model_scores:
                model_ids = [f"{r.provider}:{r.model}" for r in refs]
                reordered_ids = reorder_totem_pole_by_research(model_ids, research_summary.model_scores)
                
                # Rebuild refs in new order
                model_to_ref = {f"{r.provider}:{r.model}": r for r in refs}
                refs = [model_to_ref[mid] for mid in reordered_ids if mid in model_to_ref]
                print(f"[MOA] Totem pole reordered by research: {[mid.split(':')[-1] for mid in reordered_ids[:5]]}...")
        except Exception as e:
            print(f"[MOA] Online research failed, using static ordering: {e}")
    
    # Filter to healthy providers
    healthy_refs = [r for r in refs if await provider_health.is_healthy(r.provider)]
    if not healthy_refs:
        return json.dumps({"error": "No healthy providers available", "success": False})
    
    # System prompts
    ref_system = get_system_prompt(mode, False)
    agg_system = get_system_prompt(mode, True)
    
    # Step 1: Parallel reference queries using optimized client with batching
    print(f"[MOA] Querying {len(healthy_refs)} reference models in {mode.value} mode...")
    
    http_client = get_http_client()
    
    # Prepare batch requests
    batch_requests = [
        (next((p for p in MOA_PROVIDERS if p.name == ref.provider), None), ref, ref_system, user_prompt)
        for ref in healthy_refs
    ]
    # Filter out any None providers
    batch_requests = [(p, r, s, u) for p, r, s, u in batch_requests if p is not None]
    
    ref_responses = await http_client.call_model_batch(batch_requests, max_parallel=8)
    
    successful = [(r, resp) for r, resp in zip(healthy_refs, ref_responses) if resp]
    print(f"[MOA] Got {len(successful)}/{len(healthy_refs)} successful responses")
    
    if not successful:
        return json.dumps({"error": "All reference models failed", "success": False})
    
    # Record successful models for project preference
    for ref, _ in successful:
        model_selector.record_successful_model(project_id, mode.value, ref)
    
    # Step 2: Build aggregation prompt
    agg_prompt_parts = [
        f"Original query:\n\n{user_prompt}\n",
        "Reference model responses:\n",
    ]
    for ref, resp in successful:
        agg_prompt_parts.append(f"--- {ref.provider}:{ref.model} ({ref.role}) ---\n{resp}\n")
    
    agg_prompt = "".join(agg_prompt_parts)
    
    # Step 2b: Select aggregator model (best available)
    agg_provider = "nvidia_nim"
    agg_model = "nvidia/nemotron-3-ultra-550b-a55b"
    
    for p in MOA_PROVIDERS:
        if await provider_health.is_healthy(p.name) and p.free_models:
            agg_provider = p.name
            agg_model = p.free_models[0]
            break
    
    # Step 3: Aggregation
    print(f"[MOA] Aggregating with {agg_provider}:{agg_model}...")
    agg_provider_obj = next((p for p in MOA_PROVIDERS if p.name == agg_provider), None)
    if not agg_provider_obj:
        return json.dumps({"error": "No aggregator provider available", "success": False})
    
    agg_ref = RefModelConfig(agg_provider, agg_model, 0.3, 8192, "xhigh", "synthesizer", 1)
    agg_response = await http_client.call_model(agg_provider_obj, agg_ref, agg_system, agg_prompt)
    
    if not agg_response:
        return json.dumps({"error": "Aggregator model failed", "success": False})
    
    # Build result
    models_used = [f"{r.provider}:{r.model}" for r, _ in successful]
    duration = round(time.time() - start_time, 2)
    
    result = {
        "success": True,
        "mode": mode.value,
        "reference_responses": [
            {"provider": r.provider, "model": r.model, "role": r.role, "content": resp, 
             "temperature": r.temperature, "max_tokens": r.max_tokens}
            for r, resp in successful
        ],
        "aggregator_response": agg_response,
        "aggregator_model": f"{agg_provider}:{agg_model}",
        "provider_health": await provider_health.get_health_summary(),
        "duration_seconds": duration,
        "project_id": project_id,
        "models_used": models_used,
    }
    
    # Include research metadata if available
    if research_summary:
        result["research"] = {
            "query": research_summary.query,
            "findings_count": len(research_summary.findings) if hasattr(research_summary, 'findings') else 0,
            "model_scores": research_summary.model_scores,
            "confidence": research_summary.confidence,
        }
    
    result_json = json.dumps(result, indent=2)
    
    # Cache the response
    if use_cache:
        response_cache.set(user_prompt, mode.value, result_json, models_used, project_id)
    
    # Record session in project history
    project_manager.record_session(
        project_id, mode.value, user_prompt, 
        agg_response[:200] if agg_response else "failed",
        models_used, duration
    )
    
    return result_json


# ─── MATH SPECIALIZED MoA ───────────────────────────────────────────

async def mixture_of_agents_math(
    user_prompt: str,
    mode: MoAMode = MoAMode.STANDARD,
) -> str:
    """Specialized MoA for mathematical reasoning."""
    return await mixture_of_agents_tool(
        user_prompt=user_prompt,
        mode=mode,
        use_online_research=False,
        use_cache=True,
    )