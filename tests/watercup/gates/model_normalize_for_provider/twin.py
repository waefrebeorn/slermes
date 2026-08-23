"""twin for model_normalize_for_provider.

Traces the LIVE hermes_cli/model_normalize.py::normalize_model_for_provider
with the same dispatch-ladder rung order as the C harness (which links the
REAL src/hermes_cli/model_normalize.c port). Input format
"<provider>\\t<model_input>". VSTR(final normalized id) is included, so any
C/LIVE value divergence fails the cup automatically.
"""
import os, sys
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup

_spec = ilu.spec_from_file_location("wc_live_mn", os.path.join(DEV, "hermes_cli", "model_normalize.py"))
_mn = ilu.module_from_spec(_spec)
sys.modules["wc_live_mn"] = _mn
_spec.loader.exec_module(_mn)

# LIVE provider-alias resolution (same lazy import the module performs)
try:
    from hermes_cli.models import normalize_provider as _np
except Exception:
    _np = None

AGGREGATORS = {"openrouter", "nous", "kilocode"}
MATCHING_PREFIX = {"zai", "kimi-coding", "kimi-coding-cn", "minimax", "minimax-oauth",
                   "minimax-cn", "alibaba", "qwen-oauth", "xiaomi", "arcee",
                   "ollama-cloud", "custom", "gemini", "xai"}
STRIP_VENDOR_ONLY = {"copilot", "copilot-acp", "openai-codex"}
AUTHORITATIVE = {"huggingface"}
LOWERCASE = {"xiaomi"}


def norm_provider(p):
    raw = (p or "").strip().lower()
    if not raw:
        return raw
    return _np(raw) if _np else raw


def _prefix_matches(name, target):
    slash = name.find("/")
    if slash <= 0 or slash == len(name) - 1:
        return False
    return norm_provider(name[:slash]) == norm_provider(target)


def trace(provider, model_input):
    c = Cup("normalize_model_for_provider")
    name = (model_input or "").strip()

    c.branch(not name)                                       # B1
    pv = norm_provider(provider)

    c.branch(pv in AGGREGATORS)                              # B2
    oc = pv in ("opencode-zen", "opencode-go")
    c.branch(oc)                                             # B3
    if oc:
        c.branch("/" in name and name.split("/", 1)[1].strip() != "")   # B4
        bare_after = name.split("/", 1)[1] if "/" in name else name
        c.branch(pv == "opencode-zen" and bare_after.lower().startswith("claude-"))  # B5
    else:
        c.branch(pv == "anthropic")                          # B6
        if pv == "anthropic":
            c.branch(not _prefix_matches(name, pv) and "/" in name)         # B7
        else:
            c.branch(pv in STRIP_VENDOR_ONLY)                # B8
            if pv in STRIP_VENDOR_ONLY:
                c.branch(pv in ("copilot", "copilot-acp"))   # B9
                c.branch(not _prefix_matches(name, pv) and name.startswith("openai/"))  # B10
            else:
                c.branch(pv == "deepseek")                   # B11
                if pv == "deepseek":
                    c.branch(not _prefix_matches(name, pv) and "/" in name)     # B12
                else:
                    c.branch(pv in MATCHING_PREFIX)          # B13
                    if pv in MATCHING_PREFIX:
                        c.branch(pv in LOWERCASE)            # B14
                    else:
                        c.branch(pv in AUTHORITATIVE)        # B15

    result = _mn.normalize_model_for_provider(model_input, provider)
    c.valstr(result)
    return c.finalize()


def run(inputs):
    cups = []
    for raw in inputs:
        provider, _, model = raw.replace("\\t", "\t").partition("\t")
        cups.append(trace(provider, model))
    return cups
