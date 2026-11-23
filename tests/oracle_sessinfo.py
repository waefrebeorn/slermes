#!/usr/bin/env python3
"""Differential oracle for gateway/session_info_route.py (gate sessinfo).

Loads the LIVE channel_override_route via importlib and drives it with scripted
collaborators injected into sys.modules (gateway.run resolvers and the
hermes_cli provider-identity helper). The fixture supplies the scripted results;
the C harness's `out` triple is compared with the live triple.

Input (harness output, one JSON per line):
    {"func":"route", "source":..., "config_present":bool, "override":...,
     "runtime":..., "runtime_raises":bool, "identity":..., "out":[m,route,f]}
Summary: ORACLE sessinfo: N cases, M mismatches.
"""
import importlib.util
import json
import sys
import types
from types import SimpleNamespace as NS

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)


def _seed(name, **attrs):
    mod = types.ModuleType(name)
    mod.__path__ = []
    for k, v in attrs.items():
        setattr(mod, k, v)
    sys.modules[name] = mod
    parts = name.split(".")
    for i in range(1, len(parts)):
        parent = ".".join(parts[:i])
        if parent in sys.modules and not hasattr(sys.modules[parent], parts[i]):
            setattr(sys.modules[parent], parts[i], mod)
    return mod


# Bootstrap parent packages for the module's inline (function-body) imports.
try:
    import gateway  # noqa: F401
except Exception:
    _seed("gateway")
try:
    import hermes_cli  # noqa: F401
except Exception:
    _seed("hermes_cli")

SRC = QUARRY + "/gateway/session_info_route.py"
_spec = importlib.util.spec_from_file_location("sessinfo_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["sessinfo_live"] = live
_spec.loader.exec_module(live)


def canon(x):
    """Canonicalize a live result for JSON-value comparison."""
    if isinstance(x, (list, tuple)):
        return [canon(v) for v in x]
    if isinstance(x, dict):
        return {k: canon(v) for k, v in x.items()}
    return x


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
        if rec.get("func") != "route":
            continue
        cases += 1

        override = rec.get("override")
        runtime = rec.get("runtime")
        raises = bool(rec.get("runtime_raises"))
        identity = rec.get("identity")

        def getch(config, platform, chat, thread_id=None, parent_id=None):
            if override is None:
                return None
            return NS(model=override.get("model"), provider=override.get("provider"))

        def resolve(provider, target_model=None):
            if raises:
                raise RuntimeError("scripted resolver failure")
            return runtime or {}

        _seed("gateway.run", _get_channel_override=getch,
              _resolve_runtime_agent_kwargs_for_provider=resolve)
        _seed("hermes_cli.runtime_provider_custom",
              session_provider_identity=lambda *a: identity)

        cfg = object() if rec.get("config_present") else None
        src = None if rec.get("source") is None else NS(**rec["source"])
        py = canon(live.channel_override_route(cfg, src))
        got = rec.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH sessinfo case={rec.get('case')} "
                  f"C={got!r} PY={py!r}", file=sys.stderr)
    if cases == 0:
        print("ORACLE sessinfo: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE sessinfo: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
