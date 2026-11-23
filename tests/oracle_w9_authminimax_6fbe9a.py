#!/usr/bin/env python3
"""Differential oracle for hermes_cli/auth_minimax.py (w9_authminimax_6fbe9a)."""
import importlib.util
import json
import sys
import types
from datetime import datetime as RealDatetime, timezone

G = "w9_authminimax_6fbe9a"
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
if "hermes_cli" not in sys.modules:
    _hc = types.ModuleType("hermes_cli")
    _hc.__path__ = [Q + "/hermes_cli"]
    sys.modules["hermes_cli"] = _hc
_mod = types.ModuleType("hermes_cli.auth_constants")
_mod.AuthError = type("AuthError", (Exception,), {})
_mod.MINIMAX_OAUTH_GRANT_TYPE = "urn:ietf:params:oauth:grant-type:device_code"
_mod.MINIMAX_OAUTH_REFRESH_SKEW_SECONDS = 120
_mod.MINIMAX_OAUTH_SCOPE = "openid profile"
_mod._FORM_JSON_HEADERS = {}
_mod._minimax_err = lambda m, c, **kw: _mod.AuthError(m)
_mod.httpx = types.SimpleNamespace()
sys.modules["hermes_cli.auth_constants"] = _mod

SRC = Q + "/hermes_cli/auth_minimax.py"
spec = importlib.util.spec_from_file_location("authminimax_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["authminimax_live"] = live
spec.loader.exec_module(live)

NOW = [0.0]
class _FrozenDateTime(RealDatetime):
    @classmethod
    def now(cls, tz=None):
        return RealDatetime.fromtimestamp(NOW[0], tz)
live.datetime = _FrozenDateTime

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
        if func == "looks_like_ms":
            py = live._minimax_expired_in_looks_like_unix_ms(r.get("expired_in"), now_ms=r.get("now_ms"))
        elif func == "resolve":
            now = RealDatetime.fromtimestamp(r.get("now_seconds", 0), tz=timezone.utc)
            py = live._minimax_resolve_token_expiry_unix(r.get("expired_in"), now=now)
        elif func == "expiry_fields":
            NOW[0] = r.get("now_seconds", 0)
            py = live._minimax_expiry_fields(r.get("expired_in"))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
