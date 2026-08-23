"""twin for models_opencode_model_api_mode.

Traces the LIVE hermes_cli/models.py::opencode_model_api_mode with the same
rung order as the C harness. LIVE helpers opencode_provider_family and
normalize_opencode_model_id supply semantics; the prefix chain is the shared
ladder. Input format: "provider|model".
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_modname = "wc_live_hermes_cli_models"
if _modname in sys.modules:
    _models = sys.modules[_modname]
else:
    _spec = ilu.spec_from_file_location(_modname, os.path.join(DEV, "hermes_cli", "models.py"))
    _models = ilu.module_from_spec(_spec)
    sys.modules[_modname] = _models
    _spec.loader.exec_module(_models)

GO_CHAIN = [("gpt-", "codex_responses"), ("grok-", "codex_responses"),
            ("muse-spark", "codex_responses"), ("minimax-", "anthropic_messages"),
            ("qwen", "anthropic_messages")]
ZEN_CHAIN = [("claude-", "anthropic_messages"), ("gpt-", "codex_responses"),
             ("grok-", "codex_responses"), ("muse-spark", "codex_responses"),
             ("qwen", "anthropic_messages")]


def trace(inp):
    provider, _, model = inp.partition("|")
    c = Cup("opencode_model_api_mode")
    fam = _models.opencode_provider_family(provider)
    if fam == "opencode-free":
        fam = "opencode-zen"
    normalized = _models.normalize_opencode_model_id(provider, model).lower()
    if not normalized:                          # B1
        c.branch(True)
        return "chat_completions", c.finalize()
    c.branch(False)
    is_go = fam == "opencode-go"
    c.branch(is_go)                             # B2
    if is_go:
        chain = GO_CHAIN
    else:
        is_zen = fam == "opencode-zen"
        c.branch(is_zen)                        # B3
        chain = ZEN_CHAIN if is_zen else []
    result = "chat_completions"
    for pre, mode in chain:                     # LOOP prefix chain
        c.loop()
        m = normalized.startswith(pre)
        c.branch(m)
        if m:
            result = mode
            break
    return result, c.finalize()


def run(inputs):
    cups = []
    for inp in inputs:
        _, cup = trace(inp)
        cups.append(cup)
    return cups
