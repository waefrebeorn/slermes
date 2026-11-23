#!/usr/bin/env python3
"""Differential oracle for w14_webapplaun_8e9a00 (hermes_cli/webapp_launcher.py)."""
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


def _pkg(name):
    m = sys.modules.get(name)
    if m is None:
        m = _mod(name)
    if not hasattr(m, "__path__"):
        m.__path__ = []
    return m


_pkg("hermes_cli")  # parent-package bootstrap
_gw = _pkg("gateway")
_gw.__path__ = []
_mod("gateway.webapp_listener", NOT_BUILT="The Webapp is not built")

SRC = Q + "/hermes_cli/webapp_launcher.py"
spec = importlib.util.spec_from_file_location("hermes_cli.webapp_launcher_w14_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def canon(v):
    def conv(x):
        if isinstance(x, float) and x.is_integer():
            return int(x)
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


def compute(r):
    f = r.get("func")
    if f == "older_gateway":
        return live.older_gateway(r.get("response") or {})
    if f == "refusal":
        return live._refusal(r.get("response") or {})
    if f == "plain_http_note":
        return live._plain_http_note(r.get("url", ""))
    if f == "signed_out_summary":
        return live._signed_out_summary(r.get("result") or {})
    if f == "restart_applies":
        return live._restart_applies(r.get("reason"))
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
        py = canon(compute(r))
    except Exception as exc:
        print(f"LIVE ERROR {r.get('func')} case={r.get('case')}: {exc!r}",
              file=sys.stderr)
        mism += 1
        continue
    got = canon(r.get("out"))
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} case={r.get('case')} py={py!r} c={got!r}",
              file=sys.stderr)

print(f"ORACLE w14_webapplaun_8e9a00: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
