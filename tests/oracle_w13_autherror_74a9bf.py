#!/usr/bin/env python3
"""Differential oracle for hermes_cli/auth_error_copy.py (gate w13_autherror_74a9bf).

Replays the LIVE predicates and device_flow_error against reconstructed
exceptions whose observable type/attrs are carried by the harness output.
"""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
SRC = "C:/Users/eman5/hermes-agent/hermes_cli/auth_error_copy.py"
spec = importlib.util.spec_from_file_location("w13autherr_live", SRC)
live = importlib.util.module_from_spec(spec)
spec.loader.exec_module(live)

SignInCopyError = live.SignInCopyError
DeviceCodeExpired = live.DeviceCodeExpired


def make_exc(r):
    t = r.get("type", "") or ""
    msg = r.get("mess") or (t or "boom")
    oauth = r.get("oauth", "") or ""
    code = r.get("code", "") or ""
    if t == "KeyboardInterrupt":
        return KeyboardInterrupt(msg)
    if t == "EOFError":
        return EOFError(msg)
    if t == "SystemExit":
        raw = r.get("exit", "-1")
        return SystemExit(None if str(raw) == "-1" else int(raw))
    if t == "SignInCopyError":
        return SignInCopyError(msg, oauth_error_code=oauth)
    if t == "DeviceCodeExpired":
        e = DeviceCodeExpired(msg)
        e.code = code or "device_code_timeout"
        return e
    if t == "ConnectError":
        return type("ConnectError", (ConnectionError,), {})(msg)
    if t == "ReadTimeout":
        return type("ReadTimeout", (TimeoutError,), {})(msg)
    if t == "NetworkError":
        return type("NetworkError", (Exception,), {})(msg)
    if t == "ConnectionError":
        return ConnectionError(msg)
    if t == "TimeoutError":
        return TimeoutError(msg)
    e = RuntimeError(msg)
    e.code = code or None
    e.oauth_error_code = oauth
    return e


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
    got = r.get("out")
    if func == "device_flow_error":
        e = live.device_flow_error(r.get("code", ""), r.get("desc", ""),
                                   retry_command=r.get("retry", "hermes portal"))
        want = {"str": str(e), "oauth_error_code": e.oauth_error_code}
    elif func == "is_network_error":
        want = live.is_network_error(make_exc(r))
    elif func == "is_device_code_expired":
        want = live.is_device_code_expired(make_exc(r))
    elif func == "is_cancelled":
        want = live.is_cancelled(make_exc(r))
    else:
        continue
    cases += 1
    if want != got:
        mism += 1
        print(f"MISMATCH {func} in={r} py={want!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w13_autherror_74a9bf: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or cases == 0 else 0)
