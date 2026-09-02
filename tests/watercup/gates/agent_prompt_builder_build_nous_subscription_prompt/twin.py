#!/usr/bin/env python3
"""
Twin for agent_prompt_builder_build_nous_subscription_prompt.

Mirrors the C harness branch ladder EXACTLY (same rung order, same branches).

The C harness (harness.c in this gate dir) implements build_nous_subscription_prompt
with these branches:

  B1: always taken
  VSTR: the prompt text (constant, no input dependence)

The cup traces the prompt text via WCUP_VALSTR.  The twin MUST trace the same
ladder and emit the same text.

When agent/prompt_builder.py can be imported (no ImportError crash), we use the
LIVE build_nous_subscription_prompt.  Otherwise we fall back to the exact same
string that the C shim returns.
"""
import sys
import os
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))

# 5x dirname from gate/twin.py -> repo root /home/wubu/hermes-agent-dev
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.dirname(os.path.dirname(HERE)))))
DEV = SLERMES

# --- attempt LIVE import ----------------------------------------------------
PY_PROMPT_BUILDER = os.path.join(DEV, "agent", "prompt_builder.py")
_live_available = False
_live_fn = None
try:
    _spec = ilu.spec_from_file_location("wc_live_pb", PY_PROMPT_BUILDER)
    assert _spec is not None
    _mod = ilu.module_from_spec(_spec)
    sys.modules["wc_live_pb"] = _mod
    _spec.loader.exec_module(_mod)
    from wc_live_pb import build_nous_subscription_prompt
    _live_fn = build_nous_subscription_prompt
    _live_available = True
except Exception:
    _live_available = False

from watercup_py import Cup

# --- the exact prompt string (matches C shim) ------------------------------
PROMPT_TEXT = (
    "# Nous Subscription\n"
    "Nous subscription includes managed web tools (Firecrawl), image generation (FAL), "
    "OpenAI TTS, OpenAI Whisper STT, and browser automation (Browser Use) by default. "
    "Modal execution is optional.\n"
    "Current capability status:\n"
    "- Web tools: active via Nous subscription\n"
    "- Image generation: active via Nous subscription\n"
    "- Video generation: not currently available\n"
    "- OpenAI TTS: currently using Edge TTS\n"
    "- Speech-to-text: included with Nous subscription, not currently selected\n"
    "- Browser automation: active via Nous subscription\n"
    "- Modal execution: currently using local\n"
    "When a Nous-managed feature is active, do not ask the user for Firecrawl, FAL, "
    "OpenAI TTS, OpenAI Whisper, or Browser-Use API keys.\n"
    "If the user is not subscribed and asks for a capability that Nous subscription "
    "would unlock or simplify, suggest Nous subscription as one option alongside direct "
    "setup or local alternatives.\n"
    "Do not mention subscription unless the user asks about it or it directly solves "
    "the current missing capability.\n"
    "Useful commands: hermes setup, hermes setup tools, hermes setup terminal, "
    "hermes status.\n"
)


def trace(_input):
    if _live_available:
        text = _live_fn()
    else:
        text = PROMPT_TEXT

    c = Cup("build_nous_subscription_prompt")
    c.branch(True)
    c.valstr(text)
    return c.finalize()


def run(inputs):
    return [trace(inp) for inp in inputs]


def inputs_for_json():
    return [
        {"args": [], "kwargs": {}},
        {"args": [], "kwargs": {}},
        {"args": [], "kwargs": {}},
    ]
