#!/usr/bin/env python3
"""Differential oracle for hermes_cli/models_detect.py (gate w10_modelsdete_a35568)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
G = "w10_modelsdete_a35568"
sys.path.insert(0, QUARRY)

try:
    import hermes_cli  # noqa: F401
except Exception:
    pass


def _install(name, **attrs):
    mod = types.ModuleType(name)
    mod.__path__ = []
    for k, v in attrs.items():
        setattr(mod, k, v)
    sys.modules[name] = mod
    parent = name.rsplit(".", 1)[0] if "." in name else None
    if parent and parent in sys.modules:
        setattr(sys.modules[parent], name.rsplit(".", 1)[1], mod)
    return mod


SRC = QUARRY + "/hermes_cli/models_detect.py"
_spec = importlib.util.spec_from_file_location("models_detect_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["models_detect_live"] = live
_spec.loader.exec_module(live)


def _seed_models(rec):
    def normalize_provider(p):
        return rec.get("normalized_provider")

    def cached_provider_model_ids(p):
        if rec.get("catalog_error"):
            raise RuntimeError("catalog fetch failed")
        return list(rec.get("catalog", []))

    _install("hermes_cli.models",
             normalize_provider=normalize_provider,
             cached_provider_model_ids=cached_provider_model_ids,
             _AGGREGATOR_PROVIDERS=set(rec.get("aggregator_providers", [])),
             _PROVIDER_MODELS=rec.get("provider_models", {}))


def _seed_normalize(rec):
    vmap = rec.get("vendor_map", {})

    def detect_vendor(name):
        return vmap.get((name or "").lower())

    _install("hermes_cli.model_normalize", detect_vendor=detect_vendor)


def live_out(rec):
    func = rec.get("func")
    if func == "catalog_match":
        _seed_models(rec)
        return live.current_provider_catalog_match(rec.get("model_name"), rec.get("current_provider"))
    if func == "owns_vendor":
        _seed_models(rec)
        _seed_normalize(rec)
        return live.current_provider_owns_vendor(rec.get("model_name"), rec.get("current_provider"))
    return None


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except ValueError:
            continue
        if rec.get("func") not in ("catalog_match", "owns_vendor"):
            continue
        cases += 1
        py = live_out(rec)
        got = rec.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH {G} case={rec.get('case')} func={rec.get('func')} "
                  f"C={got!r} PY={py!r}", file=sys.stderr)
    if cases == 0:
        print(f"ORACLE {G}: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
