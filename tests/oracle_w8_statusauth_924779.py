#!/usr/bin/env python3
"""Differential oracle for hermes_cli/status_auth.py (w8_statusauth_924779 gate)."""
import importlib.util
import json
import os
import sys
import types

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
            pm = types.ModuleType(pn); pm.__path__ = [os.path.join(Q, *parts[:i])]; sys.modules[pn] = pm
    m = types.ModuleType(name)
    for k, v in attrs.items(): setattr(m, k, v)
    sys.modules[name] = m
    if len(parts) > 1:
        setattr(sys.modules[".".join(parts[:-1])], parts[-1], m)
    return m


stub("hermes_cli", __path__=[Q + "/hermes_cli"])
stub("hermes_cli.auth", AuthError=type("AuthError", (Exception,), {}))
stub("hermes_cli.nous_account",
     format_nous_portal_entitlement_message=lambda *a, **k: "",
     get_nous_portal_account_info=lambda *a, **k: None)
stub("hermes_cli.nous_subscription", get_nous_subscription_features=lambda *a, **k: None)
stub("tools", __path__=[Q + "/tools"])
stub("tools.tool_backend_helpers", managed_nous_tools_enabled=lambda: False)
stub("hermes_cli.config", redact_key=lambda v: "")
stub("hermes_time", safe_strftime=lambda value, fmt: value.strftime(fmt))
stub("hermes_cli.status", _row=lambda *a, **k: None, _detail=lambda *a, **k: None,
     _section=lambda *a, **k: None, _first_env_value=lambda *a, **k: "",
     _effective_provider_label=lambda: "", get_env_value=lambda *a, **k: "")

LIVE_NAME = "w8_statusauth_924779_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/hermes_cli/status_auth.py")
live = importlib.util.module_from_spec(spec)
sys.modules[LIVE_NAME] = live
spec.loader.exec_module(live)

# Pin the ".astimezone()" local conversion to UTC so the oracle is deterministic
# on hosts whose system zone is not UTC; every other live path stays intact.
import datetime as _dt
class _UtcDT(_dt.datetime):
    def astimezone(self, tz=None):
        return super().astimezone(_dt.timezone.utc)
live.datetime = _UtcDT


def compute(r):
    f = r.get("func")
    if f == "qwen_expiry":
        return live._qwen_expiry(r.get("expires_at_ms"))
    if f == "format_iso_timestamp":
        return live._format_iso_timestamp(r.get("value"))
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
    try:
        py = compute(r)
    except Exception as exc:
        print("LIVE ERROR %r: %r" % (r, exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH statusauth %r py=%r c=%r" % (r, py, got), file=sys.stderr)

print("ORACLE w8_statusauth_924779: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
