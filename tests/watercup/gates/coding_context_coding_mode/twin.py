"""twin for coding_context_coding_mode — LIVE agent/coding_context.py::_coding_mode."""
import os, sys, json
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup


def _load_live():
    try:
        spec = ilu.spec_from_file_location(
            "wc_live_cc", os.path.join(DEV, "agent", "coding_context.py"))
        mod = ilu.module_from_spec(spec)
        sys.modules["wc_live_cc"] = mod
        spec.loader.exec_module(mod)
        return mod._coding_mode
    except Exception:
        return None


_live_fn = _load_live()


def _fallback(config):
    raw = ((config or {}).get("agent", {}) or {}).get("coding_context", "auto")
    mode = str(raw).strip().lower()
    if mode in {"focus", "strict", "lean"}:
        return "focus"
    if mode in {"on", "true", "yes", "1", "always"}:
        return "on"
    if mode in {"off", "false", "no", "0", "never"}:
        return "off"
    return "auto"


def trace(raw):
    try:
        config = json.loads(raw)
    except Exception:
        config = None

    c = Cup("_coding_mode")
    obj = isinstance(config, dict)
    c.branch(obj)                                       # B1
    agent = config.get("agent") if obj else None
    has_agent = isinstance(agent, dict)
    c.branch(has_agent)                                 # B2
    mode = str(((config or {}).get("agent", {}) or {})
               .get("coding_context", "auto")).strip().lower() if obj else "auto"
    c.branch(mode in ("focus", "strict", "lean"))       # B3
    c.branch(mode in ("on", "true", "yes", "1", "always"))   # B4
    c.branch(mode in ("off", "false", "no", "0", "never"))   # B5

    result = _live_fn(config) if (_live_fn and obj) else _fallback(config)
    c.valstr(result)
    return c.finalize()


def run(inputs):
    return [trace(r) for r in inputs]
