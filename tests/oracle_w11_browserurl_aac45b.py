#!/usr/bin/env python3
"""Differential oracle for w11_browserurl_aac45b (tools/browser_url_policy.py).

The two module-level imports are stubbed; url_literals() itself is pure and
needs none of their behaviour.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


tools = _mod("tools")
tools.__path__ = []
_us = _mod("tools.url_safety", _BLOCKED_HOSTNAMES=frozenset(),
           _authority_ambiguity=lambda *a, **k: False,
           _is_always_blocked_ip=lambda *a, **k: False,
           _is_declared_fake_ip=lambda *a, **k: False,
           _parse_ip=lambda *a, **k: None,
           _url_hostname=lambda *a, **k: "",
           is_always_blocked_url=lambda *a, **k: False,
           is_safe_url=lambda *a, **k: True,
           normalize_url_for_request=lambda u: u)
_mod("tools.website_policy", check_website_access=lambda *a, **k: None)

SRC = Q + "/tools/browser_url_policy.py"
spec = importlib.util.spec_from_file_location("w11_browserurl_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w11_browserurl_live"] = live
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
        if func == "url_literals":
            py = live.url_literals(r.get("text", ""),
                                   protocol_relative=bool(r.get("protocol_relative", False)))
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
print(f"ORACLE w11_browserurl_aac45b: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
