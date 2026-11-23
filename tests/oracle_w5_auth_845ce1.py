#!/usr/bin/env python3
"""Differential oracle for acp_adapter/auth.py (noc_w5_auth_845ce1.h)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
SRC = QUARRY + "/acp_adapter/auth.py"
sys.path.insert(0, QUARRY)

if "acp_adapter" not in sys.modules:
    _pkg = types.ModuleType("acp_adapter")
    _pkg.__path__ = [QUARRY + "/acp_adapter"]
    sys.modules["acp_adapter"] = _pkg

spec = importlib.util.spec_from_file_location("auth_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["auth_live"] = live
spec.loader.exec_module(live)

ORIG_DETECT = live.detect_provider


class AuthMethodAgent(dict):
    def __init__(self, **kw):
        super().__init__(kw)


class TerminalAuthMethod(dict):
    def __init__(self, **kw):
        super().__init__(kw)


schema = types.ModuleType("acp.schema")
schema.AuthMethodAgent = AuthMethodAgent
schema.TerminalAuthMethod = TerminalAuthMethod
sys.modules["acp.schema"] = schema


def compute(r):
    f = r["func"]
    if f == "terminal_id":
        return live.TERMINAL_SETUP_AUTH_METHOD_ID
    if f == "detect_provider":
        runtime = dict(r.get("runtime") or {})
        if r.get("api_key_callable"):
            runtime["api_key"] = lambda: "token"
        rp = types.ModuleType("hermes_cli.runtime_provider")

        def resolve():
            if r.get("raise"):
                raise RuntimeError("unavailable")
            return runtime
        rp.resolve_runtime_provider = resolve
        if "hermes_cli" not in sys.modules:
            hc = types.ModuleType("hermes_cli")
            hc.__path__ = [QUARRY + "/hermes_cli"]
            sys.modules["hermes_cli"] = hc
        sys.modules["hermes_cli.runtime_provider"] = rp
        return ORIG_DETECT()
    if f == "build_auth_methods":
        live.detect_provider = lambda: r.get("provider")
        return live.build_auth_methods()
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print(f"ORACLE auth: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH auth {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE auth: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
