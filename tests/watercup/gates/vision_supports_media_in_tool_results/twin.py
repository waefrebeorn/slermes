"""twin for vision_supports_media_in_tool_results.

Traces the LIVE tools/vision_tools.py::_supports_media_in_tool_results with the
same rung order as the C harness:
  B0 empty / B1 aggregators / B2 anthropic / B3 openai /
  B4 gemini {B4a empty model, B4b gemini-3 family} / B5 default false.
The literal sets mirror the LIVE function body (they are inline literals there).
"""
import os
import sys
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_vision_tools",
    os.path.join(DEV, "tools", "vision_tools.py"))
_vt = ilu.module_from_spec(_spec)
sys.modules["wc_live_vision_tools"] = _vt
try:
    _spec.loader.exec_module(_vt)
except Exception:
    _vt = None  # heavy imports may fail headless; mirror ladder still authoritative

_AGG = {"openrouter", "nous", "vertex", "bedrock", "anthropic-vertex", "google-vertex"}
_ANT = {"anthropic", "claude", "anthropic-direct"}
_OAI = {"openai", "openai-chat", "openai-codex", "azure-openai"}
_GEM = {"google", "gemini", "google-gemini", "google-vertex-gemini"}


def trace(provider, model):
    c = Cup("supports_media_in_tool_results")
    if not provider:                                   # B0
        c.branch(True)
        res = False
        c.finalize()
        return res, c
    c.branch(False)
    if provider in _AGG:                               # B1
        c.branch(True)
        c.finalize()
        return True, c
    c.branch(False)
    if provider in _ANT:                               # B2
        c.branch(True)
        c.finalize()
        return True, c
    c.branch(False)
    if provider in _OAI:                               # B3
        c.branch(True)
        c.finalize()
        return True, c
    c.branch(False)
    if provider in _GEM:                               # B4
        c.branch(True)
        if not model:                                  # B4a
            c.branch(True)
            c.finalize()
            return False, c
        c.branch(False)
        res = ("gemini-3." in model or "gemini-3-" in model or model == "gemini-3"
               or "gemini-pro-3" in model or "gemini-flash-3" in model)  # B4b
        c.branch(res)
        c.finalize()
        return res, c
    c.branch(False)
    c.branch(False)                                    # B5
    c.finalize()
    return False, c


def run(inputs):
    cups = []
    for raw in inputs:
        provider, _, model = raw.partition("|")
        res, c = trace(provider, model)
        # cross-check against the LIVE function when importable
        if _vt is not None:
            live = _vt._supports_media_in_tool_results(provider, model)
            assert live == res, "LIVE divergence: %r/%r live=%s mirror=%s" % (
                provider, model, live, res)
        cups.append(c.finalize())
    return cups
