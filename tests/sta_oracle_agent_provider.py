#!/usr/bin/env python3
"""Oracle for agent/provider_registry.py: lower_key + configured_provider_name.

lower_key and configured_provider_name are ast-extracted verbatim. The real
configured_provider_name body runs end-to-end: its lazy imports
(hermes_cli.config.load_config_readonly, tools.tool_backend_helpers) are
pre-seeded in sys.modules so the function reads a controlled per-case config.
"""
import ast
import json
import logging
import sys
from typing import Any, Optional

SRC = "C:/Users/eman5/hermes-agent/agent/provider_registry.py"


def collect(path, wants):
    ns = {"Optional": Optional, "logging": logging, "Any": Any}
    tree = ast.parse(open(path, encoding="utf-8").read())
    funcs = []
    for n in tree.body:
        if isinstance(n, ast.FunctionDef) and n.name in wants:
            funcs.append(n)
    for n in funcs:
        exec(compile(ast.Module(body=[n], type_ignores=[]), "x", "exec"), ns)
    return {f.name: ns[f.name] for f in funcs}


NS = collect(SRC, {"lower_key", "configured_provider_name"})
lower_key = NS["lower_key"]
configured_provider_name = NS["configured_provider_name"]


def _seed_imports():
    import types

    # The config load is the IO half; return a controlled per-case value.
    _state = {"cfg": {"agent": {"provider": None}}}
    # Seed the parent packages so `from hermes_cli.config import ...` and
    # `from tools.tool_backend_helpers import ...` never reach the real (heavy)
    # package __init__ files — the leaf modules are what we substitute.
    sys.modules.setdefault("hermes_cli", types.ModuleType("hermes_cli"))
    sys.modules.setdefault("tools", types.ModuleType("tools"))
    cfg_mod = types.ModuleType("hermes_cli.config")
    cfg_mod.load_config_readonly = lambda: _state["cfg"]
    sys.modules["hermes_cli.config"] = cfg_mod
    # tools.tool_backend_helpers.NOUS_MANAGED_PROVIDER
    tb = types.ModuleType("tools.tool_backend_helpers")
    tb.NOUS_MANAGED_PROVIDER = "nous"
    sys.modules["tools.tool_backend_helpers"] = tb
    return _state


def main() -> int:
    try:
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
    state = _seed_imports()
    noop_log = logging.getLogger("oracle-noop")
    noop_log.addHandler(logging.NullHandler())

    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE agent_provider: 0 cases (link failure in disguise)")
        return 1

    mism = 0
    for c in cases:
        fam, raw = c["family"], c["in"]
        if fam == "lk":
            value = "" if raw is None else raw  # C NULL -> "";
            want = lower_key(value)
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH {c['case']}: C={c['out']!r} PY={want!r} in={raw!r}")
        elif fam == "cpn":
            # Feed the real function a config carrying the raw provider value.
            state["cfg"] = {"agent": {"provider": raw}}
            want = configured_provider_name("agent", noop_log)
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH {c['case']}: C={c['out']!r} PY={want!r} in={raw!r}")
    print(f"ORACLE agent_provider: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
