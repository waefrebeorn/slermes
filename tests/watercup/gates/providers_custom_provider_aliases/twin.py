"""twin for providers_custom_provider_aliases — LIVE
hermes_cli/providers.py::custom_provider_aliases (+ custom_provider_slug)."""
import os, sys
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup


def _load_live():
    try:
        spec = ilu.spec_from_file_location(
            "wc_live_providers", os.path.join(DEV, "hermes_cli", "providers.py"))
        mod = ilu.module_from_spec(spec)
        sys.modules["wc_live_providers"] = mod
        spec.loader.exec_module(mod)
        return mod.custom_provider_aliases
    except Exception:
        return None


_live_fn = _load_live()


def _slug(display_name, provider_key=""):
    identity = str(provider_key or "").strip() or str(display_name or "").strip()
    normalized = identity.lower().replace(" ", "-")
    return normalized if normalized.startswith("custom:") else f"custom:{normalized}"


def _fallback(display_name, provider_key=""):
    aliases = set()
    for value in (display_name, provider_key):
        raw = str(value or "").strip().lower()
        if not raw:
            continue
        normalized = raw.replace(" ", "-")
        aliases.update({raw, normalized, _slug(normalized)})
        if normalized.startswith("custom:"):
            suffix = normalized.split(":", 1)[1]
            if suffix:
                aliases.update({suffix, f"custom:{normalized}"})
    return frozenset(aliases)


def trace(raw):
    raw = raw.replace("\\t", "\t")
    parts = raw.split("\t")
    display = parts[0]
    key = parts[1] if len(parts) > 1 else ""

    c = Cup("custom_provider_aliases")
    c.branch(bool(display.strip()))                              # B1
    norm0 = display.strip().lower().replace(" ", "-")
    c.branch(norm0.startswith("custom:") and len(norm0) > 7)     # B2
    c.branch(bool(key.strip()))                                  # B3
    norm1 = key.strip().lower().replace(" ", "-")
    c.branch(norm1.startswith("custom:") and len(norm1) > 7)     # B4

    result = (_live_fn(display, key) if _live_fn
              else _fallback(display, key))
    c.valstr("|".join(sorted(result)))
    return c.finalize()


def run(inputs):
    return [trace(r) for r in inputs]
