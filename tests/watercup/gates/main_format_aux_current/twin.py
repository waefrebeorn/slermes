"""twin for main_format_aux_current — LIVE hermes_cli/main.py::_format_aux_current."""
import os, sys
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup


def _load_live():
    try:
        spec = ilu.spec_from_file_location(
            "wc_live_main", os.path.join(DEV, "hermes_cli", "main.py"))
        mod = ilu.module_from_spec(spec)
        sys.modules["wc_live_main"] = mod
        spec.loader.exec_module(mod)
        return mod._format_aux_current
    except Exception:
        return None


_live_fn = _load_live()


def _fallback(task_cfg):
    if not isinstance(task_cfg, dict):
        return "auto"
    base_url = str(task_cfg.get("base_url") or "").strip()
    provider = str(task_cfg.get("provider") or "auto").strip() or "auto"
    model = str(task_cfg.get("model") or "").strip()
    if base_url:
        short = base_url.replace("https://", "").replace("http://", "").rstrip("/")
        return f"custom ({short})" + (f" · {model}" if model else "")
    if provider == "auto":
        return "auto" + (f" · {model}" if model else "")
    if model:
        return f"{provider} · {model}"
    return provider


def trace(raw):
    raw = raw.replace("\\t", "\t")
    parts = raw.split("\t")
    base = parts[0] if parts else ""
    provider = parts[1] if len(parts) > 1 else ""
    model = parts[2] if len(parts) > 2 else ""

    c = Cup("_format_aux_current")
    c.branch(not raw)                                   # B1: empty arg
    has_base = bool(base)
    c.branch(has_base)                                  # B2
    c.branch(not has_base and (provider == "auto" or not provider))  # B3

    cfg = {"base_url": base, "provider": provider or None, "model": model}
    result = _live_fn(cfg) if _live_fn else _fallback(cfg)
    c.valstr(result)
    return c.finalize()


def run(inputs):
    return [trace(r) for r in inputs]
