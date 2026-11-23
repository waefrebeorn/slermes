#!/usr/bin/env python3
"""Differential oracle for hermes_cli/doctor_live.py (gate w8_doctorlive_4ef4c4).

Logic under test: _classify_http(name, resp, key_hint).  The HTTP response is
represented by a tiny stand-in carrying only `status_code` (a string in the
fixture, converted to int here, or None).  The LIVE classifier decides pass /
fail and the exact detail string, which is compared to the C array.
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


def _stub(name, **attrs):
    parts = name.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pm = types.ModuleType(pn)
            pm.__path__ = [Q + "/" + "/".join(parts[:i])]
            sys.modules[pn] = pm
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_stub("hermes_cli.browser_runtime", chromium_executable=lambda: None)
_stub("hermes_cli.doctor", _section=lambda *a: None, check_info=lambda *a: None)
_stub("hermes_cli.doctor_report", check_fail=lambda *a: None,
      check_ok=lambda *a: None, check_warn=lambda *a: None)
_stub("utils", normalize_proxy_env_vars=lambda: None)

LIVE_NAME = "w8_doctorlive_4ef4c4_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/hermes_cli/doctor_live.py")
live = importlib.util.module_from_spec(spec)
sys.modules[LIVE_NAME] = live
spec.loader.exec_module(live)


class Resp:
    def __init__(self, code):
        self.status_code = code


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") != "classify_http_ex":
        continue

    raw = r.get("code")
    code = None if raw is None else int(raw)
    pr = live._classify_http(r.get("name", ""), Resp(code), r.get("key_hint", ""))
    py = [pr.name, pr.status, pr.detail]

    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH classify_http_ex %r py=%r c=%r" % (r, py, got), file=sys.stderr)

print("ORACLE w8_doctorlive_4ef4c4: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
