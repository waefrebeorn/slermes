#!/usr/bin/env python3
"""Differential oracle for hermes_cli/auth_codex.py pure helpers (w8_authcodex_1aa766)."""
import importlib.util, json, os, sys, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def stub(name, **attrs):
    parts = name.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pm = types.ModuleType(pn)
            pm.__path__ = [os.path.join(Q, *parts[:i])]
            sys.modules[pn] = pm
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


class _AuthError(Exception):
    pass


httpx_stub = types.ModuleType("httpx")
httpx_stub.Response = type("Response", (), {})
httpx_stub.Client = type("Client", (), {})
stub("hermes_cli.auth_constants",
     _decode_jwt_claims=lambda token: {},
     AUTH_LOCK_TIMEOUT_SECONDS=15.0,
     AuthError=_AuthError,
     CODEX_ACCESS_TOKEN_REFRESH_SKEW_SECONDS=120,
     CODEX_OAUTH_CLIENT_ID="",
     CODEX_OAUTH_TOKEN_URL="",
     CODEX_RATE_LIMITED_CODE="codex_rate_limited",
     DEFAULT_CODEX_BASE_URL="https://chatgpt.com/backend-api/codex",
     _codex_err=lambda *a, **k: _AuthError("codex"),
     httpx=httpx_stub)
stub("hermes_cli.version_info", hermes_user_agent="hermes-test")
stub("utils", env_float=lambda name, default: default,
     normalize_proxy_env_vars=lambda d: d)
stub("agent.secret_scope",
     UnscopedSecretError=type("UnscopedSecretError", (Exception,), {}),
     get_secret_str=lambda name, default="": "")

LIVE_NAME = "w8_authcodex_1aa766_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/hermes_cli/auth_codex.py")
live = importlib.util.module_from_spec(spec)
sys.modules[LIVE_NAME] = live
spec.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "stripped":
        return live._stripped(r.get("value"))
    if f == "is_codex_rate_limit_shaped":
        return live._is_codex_rate_limit_shaped(r.get("code"), r.get("reason"), r.get("message"))
    if f == "entry_is_rate_limit_exhausted":
        return live._entry_is_rate_limit_exhausted(r.get("entry") or {})
    if f == "codex_usage_probe_url":
        return live._codex_usage_probe_url(r.get("base_url"))
    if f == "codex_quota_probe_cache_key":
        return live._codex_quota_probe_cache_key(r.get("token", ""))
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
        print("LIVE ERROR %r: %r" % (r, exc), file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %r py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE w8_authcodex_1aa766: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
