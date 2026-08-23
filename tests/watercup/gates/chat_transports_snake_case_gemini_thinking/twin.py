"""twin for chat_transports_snake_case_gemini_thinking_config — LIVE
agent/transports/chat_completions.py::_snake_case_gemini_thinking_config."""
import os, sys, json
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup


def _load_live():
    try:
        spec = ilu.spec_from_file_location(
            "wc_live_cct", os.path.join(DEV, "agent", "transports", "chat_completions.py"))
        mod = ilu.module_from_spec(spec)
        sys.modules["wc_live_cct"] = mod
        spec.loader.exec_module(mod)
        return mod._snake_case_gemini_thinking_config
    except Exception:
        return None


_live_fn = _load_live()


def _fallback(config):
    if not isinstance(config, dict) or not config:
        return None
    translated = {}
    if isinstance(config.get("includeThoughts"), bool):
        translated["include_thoughts"] = config["includeThoughts"]
    if isinstance(config.get("thinkingLevel"), str) and config["thinkingLevel"].strip():
        translated["thinking_level"] = config["thinkingLevel"].strip().lower()
    if isinstance(config.get("thinkingBudget"), (int, float)) and \
            not isinstance(config.get("thinkingBudget"), bool):
        translated["thinking_budget"] = int(config["thinkingBudget"])
    return translated or None


def trace(raw):
    try:
        config = json.loads(raw)
    except Exception:
        config = None

    c = Cup("_snake_case_gemini_thinking_config")
    obj = isinstance(config, dict) and bool(config)
    c.branch(obj)                                            # B1
    it = config.get("includeThoughts") if obj else None
    c.branch(isinstance(it, bool))                           # B2
    tl = config.get("thinkingLevel") if obj else None
    c.branch(isinstance(tl, str) and tl.strip() != "")       # B3
    tb = config.get("thinkingBudget") if obj else None
    c.branch(isinstance(tb, (int, float)) and not isinstance(tb, bool))  # B4

    result = _live_fn(config) if _live_fn else _fallback(config)
    c.valstr("" if result is None else json.dumps(result, separators=(",", ":")))
    return c.finalize()


def run(inputs):
    return [trace(r) for r in inputs]
