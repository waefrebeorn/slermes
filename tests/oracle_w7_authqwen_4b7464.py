#!/usr/bin/env python3
"""Differential oracle for hermes_cli/auth_qwen.py (w7_authqwen_4b7464)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, pkg=False, **attrs):
    m = types.ModuleType(name)
    if pkg:
        m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_mod("hermes_cli", pkg=True)
_mod("hermes_cli.auth_constants",
     AuthError=type("AuthError", (Exception,), {}),
     DEFAULT_QWEN_BASE_URL="https://portal.qwen.ai/v1",
     QWEN_ACCESS_TOKEN_REFRESH_SKEW_SECONDS=120,
     QWEN_OAUTH_CLIENT_ID="qwen-cli",
     QWEN_OAUTH_TOKEN_URL="https://portal.qwen.ai/token",
     _FORM_JSON_HEADERS={},
     _qwen_err=lambda m, c: Exception(m),
     httpx=types.SimpleNamespace(post=lambda *a, **k: None))

SRC = Q + "/hermes_cli/auth_qwen.py"
spec = importlib.util.spec_from_file_location("w7_authqwen_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w7_authqwen_live"] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    try:
        if func == "is_expiring":
            now = r.get("now", 0)
            live.time = types.SimpleNamespace(time=lambda: now)
            py = live._qwen_access_token_is_expiring(r.get("expiry"), r.get("skew", 120))
        elif func == "base_url":
            env = r.get("env_value", "")
            live.os = types.SimpleNamespace(getenv=lambda k, d="": env)
            py = live._qwen_base_url()
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w7_authqwen_4b7464: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
