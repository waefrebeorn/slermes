#!/usr/bin/env python3
"""Oracle for upstream Hermes agent/secret_scope.py (gate w6_secretscope_bc3fee).

All process/context state is injected from each fixture line: os.environ is a
plain dict, the bound scope is a SimpleNamespace, and hermes_constants is
stubbed for the launch-only-key lookup. No real environment is mutated.
"""
import importlib.util
import json
import sys
import types
from types import SimpleNamespace

QUARRY = "C:/Users/eman5/hermes-agent"
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

spec = importlib.util.spec_from_file_location(
    "live_secret_scope", QUARRY + "/agent/secret_scope.py")
live = importlib.util.module_from_spec(spec)
sys.modules["live_secret_scope"] = live
spec.loader.exec_module(live)

hc = types.ModuleType("hermes_constants")
sys.modules["hermes_constants"] = hc

Unscoped = live.UnscopedSecretError

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    if func == "strip":
        py = live._strip_inline_comment(r.get("value", ""))
    elif func == "pvalue":
        py = live._parse_env_value(r.get("raw", ""))
    elif func == "ptext":
        py = live._parse_env_text(r.get("text", ""))
    elif func == "get":
        launch_only = set(r.get("launch_only_names") or [])
        hc.is_launch_only_env_key = lambda n, s=launch_only: n in s
        launch = r.get("launch_scope")
        bound = r.get("bound")
        live.os = SimpleNamespace(environ=r.get("env") or {})
        live._bound_secrets = lambda: (SimpleNamespace(mapping=bound, profile_home=None)
                                       if bound is not None else None)
        live._MULTIPLEX_ACTIVE = bool(r.get("multiplex_active"))
        live.is_multiplex_active = lambda: bool(r.get("multiplex_active"))
        live.serves_routed_profile = lambda: bool(r.get("serves_routed"))
        live._LAUNCH_BODY_SCOPE = SimpleNamespace(
            get=lambda: ((lambda: launch) if launch is not None else None))
        try:
            py = live.get_secret(r.get("name", ""), r.get("default"))
        except Unscoped:
            py = {"error": "UnscopedSecretError"}
    else:
        continue
    got = r.get("out")
    cases += 1
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (func, r.get("case"), py, got), file=sys.stderr)

print("ORACLE w6_secretscope_bc3fee: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
